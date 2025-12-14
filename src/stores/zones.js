import { defineStore } from 'pinia';
import apiClient from '@/api';
import { useDevicesStore } from './devices';
import { useMeshesStore } from './meshes';

export const useZonesStore = defineStore('zones', {
  state: () => ({
    zones: [],
    allZones: [],
    activeZoneId: null,
    loading: false,
    error: null,
    isZoneDirty: false,
    backupVertices: null,
  }),
  getters: {
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
      this.isZoneDirty = false;
      try {
        const response = await apiClient.get(`/meshes/${meshId}/zones`);
        this.zones = response.data;
        if (this.zones.length > 0) {
          this.setActiveZone(this.zones[0].id);
        } else {
          this.activeZoneId = null;
          const devicesStore = useDevicesStore();
          devicesStore.fetchDevices({ meshId });
        }
      } catch (e) {
        this.error = 'Falha ao buscar zones.';
        this.zones = [];
        this.activeZoneId = null;
        console.error(e);
        try {
          const devicesStore = useDevicesStore();
          await devicesStore.fetchDevices({ meshId });
        } catch (e) {
          console.log(e);
        }
      } finally {
        this.loading = false;
      }
    },
    async fetchAllZones() {
      this.loading = true;
      this.error = null;
      try {
        const response = await apiClient.get('/zones');
        this.allZones = response.data;
      } catch (e) {
        this.error = 'Falha ao buscar zones.';
        console.error(e);
      } finally {
        this.loading = false;
      }
    },
    setActiveZone(zoneId) {
      if (this.activeZoneId === zoneId) return;

      this.activeZoneId = zoneId;
      this.isZoneDirty = false;

      const devicesStore = useDevicesStore();
      const meshesStore = useMeshesStore();

      if (zoneId) {
        devicesStore.fetchDevices({ meshId: meshesStore.selectedMeshId });
      } else {
        devicesStore.clearDevices();
      }
    },
    updateLocalZone(zoneId, zoneData) {
      const index = this.zones.findIndex(z => z.id === zoneId);
      if (index !== -1) {
        this.zones[index] = { ...this.zones[index], ...zoneData };
        this.isZoneDirty = true;
      }
    },
    async updateZone(zoneId, zoneData) {
      this.loading = true;
      this.error = null;
      try {
        const response = await apiClient.patch(`/zones/${zoneId}`, zoneData);
        const index = this.zones.findIndex(z => z.id === zoneId);
        if (index !== -1) {
          this.zones[index] = response.data;
          this.isZoneDirty = false;
        }
      } catch (e) {
        this.error = 'Falha ao atualizar zone.';
        console.error(e);
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