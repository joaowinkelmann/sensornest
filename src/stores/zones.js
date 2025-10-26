import { defineStore } from 'pinia';
import apiClient from '@/api';
import { useDevicesStore } from './devices';
import { useMeshesStore } from './meshes'; // added import

export const useZonesStore = defineStore('zones', {
  state: () => ({
    zones: [],
    activeZoneId: null, // To track the currently selected zone
    loading: false,
    error: null,
    isZoneDirty: false, // Track if the active zone has unsaved changes
    backupVertices: null,
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
        const devicesStore = useDevicesStore();
        devicesStore.clearDevices();
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
    async fetchAllZones() {
      this.loading = true;
      this.error = null;
      try {
        const response = await apiClient.get('/zones');
        this.zones = response.data;
      } catch (e) {
        this.error = 'Failed to fetch all zones.';
        console.error(e);
      } finally {
        this.loading = false;
      }
    },
    // Action to set the active zone
    setActiveZone(zoneId) {
      if (this.activeZoneId === zoneId) return; // Avoid re-fetching if already active

      this.activeZoneId = zoneId;
      this.isZoneDirty = false; // Reset dirty state when changing zone

      const devicesStore = useDevicesStore();
      const meshesStore = useMeshesStore();

      if (zoneId) {
        // FIX: pass options object so it hits /meshes/{meshId}/devices
        devicesStore.fetchDevices({ meshId: meshesStore.selectedMeshId });
      } else {
        devicesStore.clearDevices();
      }
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
    async createZone(zoneData) {
      this.loading = true;
      this.error = null;
      try {
        const response = await apiClient.post('/zones', zoneData);
        this.zones.push(response.data);
        this.setActiveZone(response.data.id);
        this.isZoneDirty = true;
        return response.data;
      } catch (e) {
        this.error = 'Failed to create zone.';
        console.error(e);
        throw e;
      } finally {
        this.loading = false;
      }
    },
    backupActiveZone() {
      if (this.activeZone) {
        this.backupVertices = JSON.parse(JSON.stringify(this.activeZone.vertices || []));
      }
    },
    restoreActiveZone() {
      if (this.activeZone && this.backupVertices) {
        this.updateLocalZone(this.activeZone.id, { vertices: this.backupVertices });
        this.isZoneDirty = false;
        this.backupVertices = null;
      }
    },
  },
});