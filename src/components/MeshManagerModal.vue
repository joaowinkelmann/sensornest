<template>
  <q-dialog :model-value="show" persistent>
    <q-card style="width: 400px">
      <q-card-section>
        <div class="text-h6">Select or Create a Mesh</div>
      </q-card-section>
      <q-card-section>
        <div v-if="meshesStore.meshes.length > 0">
          <q-list bordered>
            <q-item v-for="mesh in meshesStore.meshes" :key="mesh.id" clickable @click="selectMesh(mesh.id)">
              <q-item-section>{{ mesh.name }}</q-item-section>
            </q-item>
          </q-list>
        </div>
        <div v-else class="text-grey-6">No meshes found. Create one below.</div>
        <q-input v-model="newMeshName" label="New Mesh Name" class="q-mt-md" />
        <q-btn label="Create Mesh" color="primary" class="q-mt-sm" @click="createMesh" :loading="loading" />
      </q-card-section>
    </q-card>
  </q-dialog>
</template>

<script setup>
import { ref, onMounted } from 'vue';
import { useMeshesStore } from '@/stores/meshes';

const props = defineProps({ show: Boolean });
const emit = defineEmits(['mesh-selected']);

const meshesStore = useMeshesStore();
const newMeshName = ref('');
const loading = ref(false);

onMounted(() => {
  meshesStore.fetchMeshes();
});

const selectMesh = (id) => {
  emit('mesh-selected', id);
};

const createMesh = async () => {
  if (!newMeshName.value) return;
  loading.value = true;
  try {
    // You need to implement createMesh in your store or call your API here
    const response = await meshesStore.createMesh(newMeshName.value);
    emit('mesh-selected', response.id);
    newMeshName.value = '';
  } finally {
    loading.value = false;
  }
};
</script>