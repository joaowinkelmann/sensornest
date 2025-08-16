import { defineStore } from 'pinia'
import apiClient from '@/api'

export const useDevicesStore = defineStore('devices', {
  state: () => ({
    devices: [],
    loading: false,
    error: null,
  }),
  actions: {
    async fetchDevices(zoneId) {
      if (!zoneId) {
        this.devices = []
        return
      }
      this.loading = true
      this.error = null
      try {
        const response = await apiClient.get(`/zones/${zoneId}/devices`)
        this.devices = response.data
      } catch (e) {
        this.error = 'Failed to fetch devices.'
        this.devices = []
        console.error(e)
      } finally {
        this.loading = false
      }
    },
    clearDevices() {
      this.devices = []
    },
  },
})