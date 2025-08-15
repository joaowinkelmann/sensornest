import { defineStore } from 'pinia';
import apiClient from '@/api';

export const useZonesStore = defineStore('zones', {
  state: () => ({
    zones: [],
    activeZoneId: null, // To track the currently selected zone
    loading: false,
    error: null,
    isZoneDirty: false, // Track if the active zone has unsaved changes
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
      this.isZoneDirty = false; // Reset dirty state on fetch
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
      this.isZoneDirty = false; // Reset dirty state when changing zone
    },
    // Action to optimistically update a zone locally
    updateLocalZone(zoneId, zoneData) {
      const index = this.zones.findIndex(z => z.id === zoneId);
      if (index !== -1) {
        this.zones[index] = { ...this.zones[index], ...zoneData };
        this.isZoneDirty = true; // Mark as dirty
      }
    },
    // Action to update a zone's data (e.g., adding a vertex)
    async updateZone(zoneId, zoneData) {
      this.loading = true;
      this.error = null;
      try {
        const response = await apiClient.patch(`/zones/${zoneId}`, zoneData);
        const index = this.zones.findIndex(z => z.id === zoneId);
        if (index !== -1) {
          this.zones[index] = response.data;
          this.isZoneDirty = false; // Mark as clean after successful save
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
        this.isZoneDirty = true; // A new zone is considered dirty until its points are saved
      } catch (e) {
        this.error = 'Failed to create zone.';
        console.error(e);
      } finally {
        this.loading = false;
      }
    },
  },
});