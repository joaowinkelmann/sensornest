import { defineStore } from 'pinia';
import apiClient from '@/api';

export const useDataExporterStore = defineStore('dataExporter', {
  state: () => ({
    loading: false,
    error: null,
  }),
  actions: {
    async exportData(entityType, entityId, overrides = {}) {
      this.loading = true;
      this.error = null;
      try {
        // chunks de 5mb por default
        const payload = {
          entityType,
          entityId,
          maxBytesPerFile: 5 * 1024 * 1024, // 5 MiB
          ...overrides,
        };

        const response = await apiClient.post('/data-exporter/export', payload);
        return response.data;
      } catch (e) {
        this.error = 'Falha ao exportar dados.';
        console.error(e);
        throw e;
      } finally {
        this.loading = false;
      }
    },
  },
});
