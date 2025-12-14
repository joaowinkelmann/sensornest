import { defineStore } from 'pinia';
import apiClient from '@/api';
import { useZonesStore } from './zones';
import { useDevicesStore } from './devices';

export const useMeshesStore = defineStore('meshes', {
  state: () => ({
    meshes: [],
    selectedMeshId: null,
    loading: false,
    error: null,
  }),
  getters: {
    selectedMesh: (state) => {
      return state.meshes.find(m => m.id === state.selectedMeshId) || null;
    }
  },
  actions: {
    async fetchMeshes() {
      this.loading = true;
      this.error = null;
      try {
        const response = await apiClient.get('/meshes');
        this.meshes = response.data;
        if (this.meshes.length > 0 && !this.selectedMeshId) {
          this.selectMesh(this.meshes[0].id);
        }
      } catch (e) {
        this.error = 'Falha ao buscar meshes.';
        console.error(e);
      } finally {
        this.loading = false;
      }
    },
    selectMesh(meshId) {
      this.selectedMeshId = meshId;
      const zonesStore = useZonesStore();
      const devicesStore = useDevicesStore();
      devicesStore.clearDevices();
      zonesStore.fetchZones(meshId);
    },

    async createMesh(meshData) {
      this.loading = true;
      try {
        const response = await apiClient.post('/meshes', meshData);
        this.meshes.push(response.data);
        return response.data;
      } catch (e) {
        this.error = 'Falha ao criar mesh.';
        console.error(e);
        throw e;
      } finally {
        this.loading = false;
      }
    },

    async updateMesh(meshId, meshData) {
      this.loading = true;
      try {
        const response = await apiClient.patch(`/meshes/${meshId}`, meshData);
        const index = this.meshes.findIndex(m => m.id === meshId);
        if (index !== -1) {
          this.meshes[index] = response.data;
        }
        return response.data;
      } catch (e) {
        this.error = 'Falha ao atualizar mesh.';
        console.error(e);
        throw e;
      } finally {
        this.loading = false;
      }
    },

    async uploadMapImage(file, meshId = this.selectedMeshId) {
      if (!meshId) {
        throw new Error('meshId is required to upload a map image');
      }

      const formData = new FormData();
      formData.append('file', file);
      formData.append('meshId', meshId);

      try {
        const response = await apiClient.post('/meshes/upload-map', formData, {
          headers: {
            'Content-Type': 'multipart/form-data',
          },
        });

        const index = this.meshes.findIndex(m => m.id === meshId);
        if (index !== -1) {
          this.meshes[index] = { ...this.meshes[index], ...response.data };
        }

        return response.data;
      } catch (e) {
        console.error('Falha ao carregar imagem do mapa:', e);
        throw e;
      }
    },
  },
});