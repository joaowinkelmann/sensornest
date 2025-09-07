<template>
  <q-page class="q-pa-md">
    <div class="row items-center justify-between q-mb-md">
      <div class="text-h4">Meshes</div>
      <q-btn color="primary" icon="add" label="Create Mesh" @click="showCreateDialog = true" />
    </div>

    <q-list bordered separator>
      <q-item v-for="mesh in meshesStore.meshes" :key="mesh.id" clickable v-ripple @click="selectMesh(mesh.id)">
        <q-item-section>
          <q-item-label>{{ mesh.name }}</q-item-label>
          <q-item-label caption>{{ mesh.id }}</q-item-label>
        </q-item-section>
        <q-item-section side>
          <q-btn flat round icon="edit" @click.stop="showEditDialog(mesh)" />
        </q-item-section>
      </q-item>
    </q-list>

    <MeshDialog
      :show="showCreateDialog || !!editingMesh"
      :mesh-to-edit="editingMesh"
      @close="closeDialogs"
      @mesh-created="handleMeshCreated"
      @mesh-updated="handleMeshUpdated"
    />
  </q-page>
</template>

<script setup>
import { ref, onMounted } from 'vue';
import { useMeshesStore } from '@/stores/meshes';
import { useRouter } from 'vue-router';
import { useQuasar } from 'quasar';
import MeshDialog from '@/components/MeshDialog.vue';

const meshesStore = useMeshesStore();
const router = useRouter();
const $q = useQuasar();

const showCreateDialog = ref(false);
const editingMesh = ref(null);

onMounted(() => {
  meshesStore.fetchMeshes();
});

const selectMesh = (meshId) => {
  meshesStore.selectMesh(meshId);
  router.push('/map-editor');
};

const showEditDialog = (mesh) => {
  editingMesh.value = mesh;
};

const closeDialogs = () => {
  showCreateDialog.value = false;
  editingMesh.value = null;
};

const handleMeshCreated = (newMesh) => {
  $q.notify({
    type: 'positive',
    message: `Mesh "${newMesh.name}" created successfully!`,
  });
  closeDialogs();
  meshesStore.fetchMeshes();
};

const handleMeshUpdated = (updatedMesh) => {
  $q.notify({
    type: 'positive',
    message: `Mesh "${updatedMesh.name}" updated successfully!`,
  });
  closeDialogs();
  meshesStore.fetchMeshes();
};
</script>
