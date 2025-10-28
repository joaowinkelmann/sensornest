<template>
  <q-page class="q-pa-md">
    <div class="row items-center justify-between q-mb-md">
      <div class="text-h4">{{ t('meshes.title') }}</div>
      <q-btn color="primary" icon="add" :label="t('meshes.createButton')" @click="showCreateDialog = true" />
    </div>

    <q-list bordered separator>
      <q-item v-for="mesh in meshesStore.meshes" :key="mesh.id">
        <q-item-section clickable v-ripple @click="selectMesh(mesh.id)">
          <q-item-label>{{ mesh.name }}</q-item-label>
          <q-item-label caption>{{ mesh.id }}</q-item-label>
        </q-item-section>
        <q-item-section side>
          <q-btn flat round icon="edit" @click.stop="showEditDialog(mesh)" />
          <q-btn flat round icon="bar_chart" @click.stop="showStatistics(mesh)" />
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

    <q-dialog v-model="showStatsDialog">
      <q-card>
        <q-card-section>
          <div class="text-h6">{{ t('meshes.statisticsTitle', { meshName: selectedMesh?.name }) }}</div>
        </q-card-section>

        <q-card-section v-if="loading">
          <q-spinner-dots size="50px" />
        </q-card-section>

        <q-card-section v-if="error">
          <p class="text-negative">{{ t('meshes.statisticsError') }}</p>
        </q-card-section>

        <q-card-section v-if="statistics">
          <pre>{{ statistics }}</pre>
          <ZoneStatistics :meshId="selectedMesh.id" />
        </q-card-section>

        <q-card-actions align="right">
          <q-btn flat :label="t('meshes.closeButton')" color="primary" v-close-popup />
        </q-card-actions>
      </q-card>
    </q-dialog>
  </q-page>
</template>

<script setup>
import { ref, onMounted } from 'vue';
import { useMeshesStore } from '@/stores/meshes';
import { useRouter } from 'vue-router';
import { useQuasar } from 'quasar';
import { useStatistics } from '@/composables/useStatistics';
import MeshDialog from '@/components/MeshDialog.vue';
import ZoneStatistics from '@/components/ZoneStatistics.vue';
import { useI18n } from 'vue-i18n';

const { t } = useI18n();
const meshesStore = useMeshesStore();
const router = useRouter();
const $q = useQuasar();
const { statistics, loading, error, fetchStatistics } = useStatistics();

const showCreateDialog = ref(false);
const editingMesh = ref(null);
const showStatsDialog = ref(false);
const selectedMesh = ref(null);

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

const showStatistics = async (mesh) => {
  selectedMesh.value = mesh;
  await fetchStatistics('mesh', mesh.id);
  showStatsDialog.value = true;
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
