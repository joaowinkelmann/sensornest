import { defineStore } from 'pinia';
import apiClient from '@/api';

export const useDataExporterStore = defineStore('dataExporter', {
  state: () => ({
    loading: false,
    error: null,
  }),
  actions: {
    async exportData(entityType, entityId) {
      this.loading = true;
      this.error = null;
      try {
        const response = await apiClient.post('/data-exporter/export', {
          entityType,
          entityId,
        });
        return response.data;
      } catch (e) {
        this.error = 'Failed to export data.';
        console.error(e);
        throw e;
      } finally {
        this.loading = false;
      }
    },
  },
});
