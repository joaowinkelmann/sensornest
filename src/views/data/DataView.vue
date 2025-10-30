<template>
  <q-page class="flex column q-pa-md">
    <h1 class="text-h4">{{ t('data.title') }}</h1>
    <p>{{ t('data.description') }}</p>

    <div class="q-gutter-md q-mt-md" style="max-width: 500px">
      <q-select
        v-model="entityType"
        :options="entityTypeOptions"
        :label="t('data.entityType')"
        emit-value
        map-options
        outlined
        data-testid="entity-type-select"
      />

      <q-select
        v-model="selectedEntity"
        :options="entityOptions"
        :label="t('data.selectEntity')"
        :loading="loadingEntities"
        :disable="!entityType"
        outlined
        use-input
        @filter="filterFn"
        data-testid="entity-select"
      >
        <template v-slot:no-option>
          <q-item>
            <q-item-section class="text-grey">
              {{ t('data.noResults') }}
            </q-item-section>
          </q-item>
        </template>
      </q-select>

      <!-- Date filters -->
      <q-input
        v-model="from"
        type="datetime-local"
        :label="t('data.fromDate')"
        outlined
        clearable
        data-testid="from-input"
      />
      <q-input
        v-model="to"
        type="datetime-local"
        :label="t('data.toDate')"
        outlined
        clearable
        data-testid="to-input"
      />

      <q-btn
        @click="handleExport"
        :loading="exporterStore.loading"
        :disable="!selectedEntity"
        color="primary"
        :label="t('data.exportButton')"
        data-testid="export-button"
      />
    </div>

    <q-banner v-if="exporterStore.error" class="q-mt-md" color="negative">
      {{ exporterStore.error }}
    </q-banner>
  </q-page>
</template>

<script setup>
import { ref, computed, onMounted, watch } from 'vue';
import { useI18n } from 'vue-i18n';
import { useQuasar } from 'quasar';
import { useDataExporterStore } from '@/stores/dataExporter';
import { useMeshesStore } from '@/stores/meshes';
import { useZonesStore } from '@/stores/zones';
import { useDevicesStore } from '@/stores/devices';

const { t } = useI18n();
const $q = useQuasar();
const exporterStore = useDataExporterStore();
const meshesStore = useMeshesStore();
const zonesStore = useZonesStore();
const devicesStore = useDevicesStore();

const entityType = ref(null);
const selectedEntity = ref(null);
const allEntities = ref([]);
const entityOptions = ref([]);

// New date filters
const from = ref('');
const to = ref('');

const entityTypeOptions = [
  { label: t('data.mesh'), value: 'mesh' },
  { label: t('data.zone'), value: 'zone' },
  { label: t('data.device'), value: 'device' },
];

const loadingEntities = computed(() => {
  return meshesStore.loading || zonesStore.loading || devicesStore.loading;
});

const fetchEntities = async () => {
  if (entityType.value === 'mesh') {
    await meshesStore.fetchMeshes();
    allEntities.value = meshesStore.meshes.map(m => ({ label: m.name, value: m.id }));
  } else if (entityType.value === 'zone') {
    await zonesStore.fetchAllZones();
    allEntities.value = zonesStore.allZones.map(z => ({ label: z.name, value: z.id }));
  } else if (entityType.value === 'device') {
    await devicesStore.fetchDevices();
    allEntities.value = devicesStore.devices.map(d => ({ label: d.name, value: d.id }));
  }
  entityOptions.value = allEntities.value;
};

watch(entityType, () => {
  selectedEntity.value = null;
  allEntities.value = [];
  entityOptions.value = [];
  fetchEntities();
});

onMounted(() => {
  meshesStore.fetchMeshes();
  zonesStore.fetchAllZones();
  devicesStore.fetchDevices();
});

const filterFn = (val, update) => {
  if (val === '') {
    update(() => {
      entityOptions.value = allEntities.value;
    });
    return;
  }

  update(() => {
    const needle = val.toLowerCase();
    entityOptions.value = allEntities.value.filter(
      v => v.label.toLowerCase().indexOf(needle) > -1
    );
  });
};

const handleExport = async () => {
  if (!selectedEntity.value) return;

  // Build optional filters according to OpenAPI: ExportDataDto.from/to (ISO)
  const options = {};
  if (from.value) options.from = new Date(from.value).toISOString();
  if (to.value) options.to = new Date(to.value).toISOString();

  try {
    // Keep backward compatibility: pass options as 3rd arg
    const result = await exporterStore.exportData(
      entityType.value,
      selectedEntity.value.value,
      options
    );

    // Handle new chunked manifest: ExportResultDto.files (array of URLs)
    if (result && typeof result === 'object' && Array.isArray(result.files) && result.files.length) {
      $q.notify({
        type: 'info',
        message: t('data.downloadingChunks', { count: result.fileCount || result.files.length }),
        timeout: 2000
      });

      // Trigger one download per file (stagger to avoid pop-up blockers)
      result.files.forEach((fileUrl, idx) => {
        setTimeout(() => {
          const a = document.createElement('a');
          a.href = fileUrl;
          a.setAttribute(
            'download',
            `${entityType.value}-${selectedEntity.value.value}-part-${idx + 1}.csv`
          );
          document.body.appendChild(a);
          a.click();
          a.remove();
        }, idx * 200);
      });
      return;
    }

    // Fallback: previous behavior (single URL string)
    if (typeof result === 'string') {
      const link = document.createElement('a');
      link.href = result;
      link.setAttribute('download', `${entityType.value}-${selectedEntity.value.value}.csv`);
      document.body.appendChild(link);
      link.click();
      link.remove();
      return;
    }

    // If store returns nothing usable
    $q.notify({ type: 'warning', message: t('data.exportNoFiles') });
  } catch (error) {
    console.error('Export failed', error);
  }
};
</script>
