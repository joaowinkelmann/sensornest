import { defineStore } from 'pinia'
import apiClient from '@/api'

export const useDevicesStore = defineStore('devices', {
  state: () => ({
    devices: [],
    loading: false,
    error: null,
  }),
  actions: {
    async fetchDevices({ meshId = null, zoneId = null } = {}) {
      this.loading = true;
      this.error = null;
      try {
        let url = '/devices';
        const params = {};
        if (meshId) {
          url = `/meshes/${meshId}/devices`;
        }
        if (zoneId) {
          // This endpoint is not in the spec, but let's assume it could exist
          // or filtering is done client-side. For now, we add it as a query param.
          // A better approach would be to have a dedicated endpoint or filter on the client.
          // Based on spec, we have /zones/{zoneId}/devices
          url = `/zones/${zoneId}/devices`;
        }

        const response = await apiClient.get(url, { params });
        this.devices = response.data;
      } catch (e) {
        this.error = 'Failed to fetch devices.';
        this.devices = [];
        console.error(e);
      } finally {
        this.loading = false;
      }
    },
    clearDevices() {
      this.devices = []
    },
    async updateDevice(deviceId, payload) {
      this.loading = true;
      this.error = null;
      try {
        const response = await apiClient.patch(`/devices/${deviceId}`, payload);
        // Update the local device data
        const index = this.devices.findIndex(d => d.id === deviceId);
        if (index !== -1) {
          this.devices[index] = { ...this.devices[index], ...response.data };
        }
        return response.data;
      } catch (e) {
        this.error = 'Failed to update device.';
        console.error(e);
        throw e; // Re-throw to be caught in the component
      } finally {
        this.loading = false;
      }
    }
  },
})