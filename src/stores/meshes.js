import { defineStore } from 'pinia';
import apiClient from '@/api';
import { useZonesStore } from './zones'; // We'll need this to trigger zone fetching

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
        // Automatically select the first mesh if none is selected
        if (this.meshes.length > 0 && !this.selectedMeshId) {
          this.selectMesh(this.meshes[0].id);
        }
      } catch (e) {
        this.error = 'Failed to fetch meshes.';
        console.error(e);
      } finally {
        this.loading = false;
      }
    },
    selectMesh(meshId) {
      this.selectedMeshId = meshId;
      // When a new mesh is selected, tell the zones store to fetch its zones
      const zonesStore = useZonesStore();
      zonesStore.fetchZones(meshId);
    },

    async createMesh(meshData) {
      this.loading = true;
      try {
        const response = await apiClient.post('/meshes', meshData);
        this.meshes.push(response.data);
        return response.data;
      } catch (e) {
        this.error = 'Failed to create mesh.';
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
        this.error = 'Failed to update mesh.';
        console.error(e);
        throw e;
      } finally {
        this.loading = false;
      }
    },

    async uploadMapImage(file) {
      const formData = new FormData();
      formData.append('file', file);

      try {
        const response = await apiClient.post('/meshes/upload-map', formData, {
          headers: {
            'Content-Type': 'multipart/form-data',
          },
        });
        return response.data;
      } catch (e) {
        console.error('Failed to upload map image:', e);
        throw e;
      }
    },
  },
});