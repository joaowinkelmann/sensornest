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
          url = `/zones/${zoneId}/devices`;
        }

        const response = await apiClient.get(url, { params });
        this.devices = response.data;
      } catch (e) {
        this.error = 'Falha ao buscar dispositivos.';
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
        const index = this.devices.findIndex(d => d.id === deviceId);
        if (index !== -1) {
          this.devices[index] = { ...this.devices[index], ...response.data };
        }
        return response.data;
      } catch (e) {
        this.error = 'Falha ao atualizar dispositivo.';
        console.error(e);
        throw e;
      } finally {
        this.loading = false;
      }
    }
  },
})