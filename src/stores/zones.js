import { defineStore } from 'pinia';
import apiClient from '@/api';

export const useZonesStore = defineStore('zones', {
  state: () => ({
    zones: [],
    loading: false,
    error: null,
  }),
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
      } catch (e) {
        this.error = 'Failed to fetch zones.';
        this.zones = [];
        console.error(e);
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
      } catch (e) {
        this.error = 'Failed to create zone.';
        console.error(e);
      } finally {
        this.loading = false;
      }
    },
  },
});