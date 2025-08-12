import { defineStore } from 'pinia';
import apiClient from '@/api';

export const useZonesStore = defineStore('zones', {
  state: () => ({
    zones: [],
    activeZoneId: null, // To track the currently selected zone
    loading: false,
    error: null,
  }),
  getters: {
    // Getter to easily find the active zone object
    activeZone: (state) => {
      return state.zones.find(z => z.id === state.activeZoneId) || null;
    },
  },
  actions: {
    async fetchZones(meshId) {
      if (!meshId) {
        this.zones = [];
        return;
      }
      this.loading = true;
      this.error = null;
      try {
        // This corresponds to GET /meshes/:meshId/zones
        const response = await apiClient.get(`/meshes/${meshId}/zones`);
        this.zones = response.data;
        // Automatically select the first zone as active if available
        if (this.zones.length > 0) {
          this.setActiveZone(this.zones[0].id);
        } else {
          this.activeZoneId = null;
        }
      } catch (e) {
        this.error = 'Failed to fetch zones.';
        this.zones = [];
        this.activeZoneId = null;
        console.error(e);
      } finally {
        this.loading = false;
      }
    },
    // Action to set the active zone
    setActiveZone(zoneId) {
      this.activeZoneId = zoneId;
    },
    // Action to update a zone's data (e.g., adding a vertex)
    async updateZone(zoneId, zoneData) {
      this.loading = true;
      this.error = null;
      try {
        // This corresponds to PUT /zones/:zoneId (example endpoint)
        // Note: The backend API for this might need to be created.
        // For now, we optimistically update the local state.
        const response = await apiClient.put(`/zones/${zoneId}`, zoneData);
        const index = this.zones.findIndex(z => z.id === zoneId);
        if (index !== -1) {
          this.zones[index] = response.data;
        }
      } catch (e) {
        this.error = 'Failed to update zone.';
        console.error(e);
        // Here you might want to revert the optimistic update if the API call fails
      } finally {
        this.loading = false;
      }
    },
    async createZone(meshId, zoneData) {
      this.loading = true;
      this.error = null;
      try {
        // This corresponds to POST /meshes/:meshId/zones
        const response = await apiClient.post(`/meshes/${meshId}/zones`, zoneData);
        this.zones.push(response.data);
        // Set the newly created zone as active
        this.setActiveZone(response.data.id);
      } catch (e) {
        this.error = 'Failed to create zone.';
        console.error(e);
      } finally {
        this.loading = false;
      }
    },
  },
});