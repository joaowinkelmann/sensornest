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
  },
});