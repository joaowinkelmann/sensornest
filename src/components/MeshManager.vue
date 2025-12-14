<template>
  <div class="bg-white rounded-lg shadow p-6 mb-8">
    <h2 class="text-xl font-bold text-gray-800 mb-4">Mesh Selector</h2>
    <div v-if="meshesStore.loading" class="text-gray-500">Loading meshes...</div>
    <div v-else-if="meshesStore.meshes.length > 0">
      <select 
        @change="meshesStore.selectMesh($event.target.value)" 
        :value="meshesStore.selectedMeshId" 
        class="w-full p-2 border border-gray-300 rounded-md bg-gray-50 focus:ring-indigo-500 focus:border-indigo-500"
      >
        <option v-for="mesh in meshesStore.meshes" :key="mesh.id" :value="mesh.id">
          {{ mesh.name }}
        </option>
      </select>
    </div>
    <div v-else class="text-gray-500">
      No meshes found for this user.
    </div>
  </div>
</template>

<script setup>
import { onMounted } from 'vue';
import { useMeshesStore } from '@/stores/meshes';

const meshesStore = useMeshesStore();

onMounted(() => {
  // Busca as malhas ao montar o componente
  meshesStore.fetchMeshes();
});
</script>