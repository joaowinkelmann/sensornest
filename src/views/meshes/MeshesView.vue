<template>
  <q-page class="q-pa-md">
    <div class="row items-center justify-between q-mb-md">
      <div class="text-h4">{{ t('meshes.title') }}</div>
      <q-btn color="primary" icon="add" :label="t('meshes.createButton')" @click="showCreateDialog = true" />
    </div>

    <q-list bordered separator>
      <div v-for="mesh in meshesStore.meshes" :key="mesh.id">
        <q-item>
          <q-item-section clickable v-ripple @click="selectMesh(mesh.id)">
            <q-item-label>{{ mesh.name }}</q-item-label>
            <q-item-label caption>{{ mesh.id }}</q-item-label>
          </q-item-section>
          <q-item-section side>
            <q-btn flat round icon="edit" @click.stop="showEditDialog(mesh)" />
            <q-btn flat round icon="bar_chart" @click.stop="toggleStatistics(mesh)" />
          </q-item-section>
        </q-item>
        <q-slide-transition>
          <div v-show="isStatisticsVisible(mesh.id)">
            <q-separator />
            <MeshStatistics :mesh="mesh" :expanded="isStatisticsVisible(mesh.id)" />
          </div>
        </q-slide-transition>
      </div>
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
import MeshStatistics from '@/components/MeshStatistics.vue';
import { useI18n } from 'vue-i18n';

const { t } = useI18n();
const meshesStore = useMeshesStore();
const router = useRouter();
const $q = useQuasar();

const showCreateDialog = ref(false);
const editingMesh = ref(null);
const expandedMeshId = ref(null);

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

const toggleStatistics = (mesh) => {
  if (expandedMeshId.value === mesh.id) {
    expandedMeshId.value = null;
  } else {
    expandedMeshId.value = mesh.id;
  }
};

const isStatisticsVisible = (meshId) => {
  return expandedMeshId.value === meshId;
};

const closeDialogs = () => {
  showCreateDialog.value = false;
  editingMesh.value = null;
};

const handleMeshCreated = (newMesh) => {
  $q.notify({
    type: 'positive',
    message: t('meshes.meshCreatedSuccess', { meshName: newMesh.name }),
  });
  closeDialogs();
  meshesStore.fetchMeshes();
};

const handleMeshUpdated = (updatedMesh) => {
  $q.notify({
    type: 'positive',
    message: t('meshes.meshUpdatedSuccess', { meshName: updatedMesh.name }),
  });
  closeDialogs();
  meshesStore.fetchMeshes();
};
</script>
