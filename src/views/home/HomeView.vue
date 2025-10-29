<template>
  <q-page class="q-pa-md">
    <!-- Overview Statistics -->
    <div class="text-h4 q-mb-md">{{ t('home.overview.title') }}</div>
    
    <!-- Loading State -->
    <div v-if="loading" class="flex flex-center q-pa-xl">
      <q-spinner-dots color="primary" size="50px" />
    </div>

    <!-- Error State -->
    <div v-else-if="error" class="q-pa-md">
      <q-banner class="bg-negative text-white" rounded>
        <template v-slot:avatar>
          <q-icon name="error" />
        </template>
        {{ t('home.overview.error') }}
      </q-banner>
    </div>

    <!-- Statistics Cards -->
    <div v-else class="row q-col-gutter-md">
      <!-- Total Devices Card -->
      <div class="col-12 col-sm-6 col-md-4">
        <q-card class="stat-card">
          <q-card-section>
            <div class="row items-center">
              <div class="col">
                <div class="stat-icon-wrapper bg-primary">
                  <q-icon name="developer_board" size="32px" color="white" />
                </div>
              </div>
            </div>
          </q-card-section>
          <q-card-section>
            <div class="stat-value">{{ statistics?.devices?.total || 0 }}</div>
            <div class="stat-label">{{ t('home.overview.totalDevices') }}</div>
          </q-card-section>
          <q-separator />
          <q-card-section class="stat-details">
            <div class="row q-col-gutter-sm">
              <div class="col-6">
                <div class="detail-label">{{ t('home.overview.active') }}</div>
                <div class="detail-value text-positive">
                  {{ statistics?.devices?.active || 0 }}
                </div>
              </div>
              <div class="col-6">
                <div class="detail-label">{{ t('home.overview.inactive') }}</div>
                <div class="detail-value text-grey">
                  {{ statistics?.devices?.inactive || 0 }}
                </div>
              </div>
            </div>
          </q-card-section>
        </q-card>
      </div>

      <!-- All Time Readings Card -->
      <div class="col-12 col-sm-6 col-md-4">
        <q-card class="stat-card">
          <q-card-section>
            <div class="row items-center">
              <div class="col">
                <div class="stat-icon-wrapper bg-purple">
                  <q-icon name="analytics" size="32px" color="white" />
                </div>
              </div>
            </div>
          </q-card-section>
          <q-card-section>
            <div class="stat-value">{{ formatNumber(statistics?.readings?.allTime) }}</div>
            <div class="stat-label">{{ t('home.overview.allTimeReadings') }}</div>
          </q-card-section>
          <q-separator />
          <q-card-section class="stat-details">
            <div class="row q-col-gutter-sm">
              <div class="col-6">
                <div class="detail-label">{{ t('home.overview.lastHour') }}</div>
                <div class="detail-value">
                  {{ formatNumber(statistics?.readings?.lastHour) }}
                </div>
              </div>
              <div class="col-6">
                <div class="detail-label">{{ t('home.overview.lastDay') }}</div>
                <div class="detail-value">
                  {{ formatNumber(statistics?.readings?.lastDay) }}
                </div>
              </div>
            </div>
          </q-card-section>
        </q-card>
      </div>

      <!-- Readings per Mesh Card -->
      <div class="col-12 col-sm-6 col-md-4">
        <q-card class="stat-card">
          <q-card-section>
            <div class="row items-center">
              <div class="col">
                <div class="stat-icon-wrapper bg-teal">
                  <q-icon name="grid_on" size="32px" color="white" />
                </div>
              </div>
            </div>
          </q-card-section>
          <q-card-section>
            <div class="stat-value">{{ statistics?.readingsPerMesh?.length || 0 }}</div>
            <div class="stat-label">{{ t('home.overview.activeMeshes') }}</div>
          </q-card-section>
          <q-separator />
          <q-card-section class="stat-details">
            <q-list dense>
              <q-item 
                v-for="mesh in statistics?.readingsPerMesh" 
                :key="mesh.meshId"
                class="mesh-item"
              >
                <q-item-section>
                  <q-item-label class="mesh-name">{{ mesh.meshName }}</q-item-label>
                </q-item-section>
                <q-item-section side>
                  <q-chip size="sm" color="primary" text-color="white">
                    {{ mesh.count }}
                  </q-chip>
                </q-item-section>
              </q-item>
            </q-list>
          </q-card-section>
        </q-card>
      </div>
    </div>

    <!-- Quick Actions -->
    <div class="q-mt-xl">
      <div class="text-h5 q-mb-md">{{ t('home.quickActions.title') }}</div>
      <div class="row q-col-gutter-md">
        <div class="col-12 col-sm-6 col-md-3" v-for="action in quickActions" :key="action.route">
          <q-card 
            class="action-card cursor-pointer" 
            @click="navigate(action.route)"
          >
            <q-card-section class="text-center">
              <q-icon :name="action.icon" size="48px" :color="action.color" />
              <div class="action-title q-mt-sm">{{ t(action.titleKey) }}</div>
              <div class="action-subtitle">{{ t(action.subtitleKey) }}</div>
            </q-card-section>
          </q-card>
        </div>
      </div>
    </div>
  </q-page>
</template>

<script setup>
import { ref, onMounted } from 'vue';
import { useI18n } from 'vue-i18n';
import { useRouter } from 'vue-router';
import apiClient from '@/api';

const router = useRouter();
const { t } = useI18n();

const statistics = ref(null);
const loading = ref(false);
const error = ref(null);

const quickActions = [
  {
    titleKey: 'home.quickActions.meshes',
    subtitleKey: 'home.quickActions.meshesSubtitle',
    icon: 'grid_on',
    color: 'primary',
    route: '/meshes'
  },
  {
    titleKey: 'home.quickActions.devices',
    subtitleKey: 'home.quickActions.devicesSubtitle',
    icon: 'developer_board',
    color: 'secondary',
    route: '/devices'
  },
  {
    titleKey: 'home.quickActions.mapEditor',
    subtitleKey: 'home.quickActions.mapEditorSubtitle',
    icon: 'map',
    color: 'accent',
    route: '/map-editor'
  },
  {
    titleKey: 'home.quickActions.reports',
    subtitleKey: 'home.quickActions.reportsSubtitle',
    icon: 'assessment',
    color: 'info',
    route: '/reports'
  }
];

const fetchStatistics = async () => {
  loading.value = true;
  error.value = null;
  try {
    const response = await apiClient.get('/statistics/overview');
    statistics.value = response.data;
  } catch (err) {
    console.error('Failed to fetch statistics:', err);
    error.value = err;
  } finally {
    loading.value = false;
  }
};

const formatNumber = (num) => {
  if (!num) return '0';
  return new Intl.NumberFormat('pt-BR').format(num);
};

const navigate = (route) => {
  if (route) {
    router.push(route);
  }
};

onMounted(() => {
  fetchStatistics();
});
</script>

<style scoped>
.stat-card {
  height: 100%;
  transition: transform 0.2s ease, box-shadow 0.2s ease;
}

.stat-card:hover {
  transform: translateY(-4px);
  box-shadow: 0 8px 24px rgba(0, 0, 0, 0.12);
}

.stat-icon-wrapper {
  width: 64px;
  height: 64px;
  border-radius: 12px;
  display: flex;
  align-items: center;
  justify-content: center;
  margin-bottom: 8px;
}

.stat-value {
  font-size: 2.5rem;
  font-weight: 700;
  line-height: 1;
  color: #1a1a1a;
  margin-bottom: 8px;
}

.stat-label {
  font-size: 1rem;
  font-weight: 500;
  color: #666;
  text-transform: uppercase;
  letter-spacing: 0.5px;
}

.stat-details {
  background: rgba(0, 0, 0, 0.02);
  padding: 12px 16px;
}

.detail-label {
  font-size: 0.75rem;
  color: #999;
  text-transform: uppercase;
  margin-bottom: 4px;
}

.detail-value {
  font-size: 1.25rem;
  font-weight: 600;
  color: #333;
}

.mesh-item {
  padding: 4px 0;
}

.mesh-name {
  font-size: 0.875rem;
  font-weight: 500;
}

.action-card {
  height: 100%;
  transition: all 0.3s ease;
  border: 2px solid transparent;
}

.action-card:hover {
  transform: translateY(-4px);
  box-shadow: 0 8px 24px rgba(0, 0, 0, 0.12);
  border-color: var(--q-primary);
}

.action-title {
  font-size: 1.125rem;
  font-weight: 600;
  color: #1a1a1a;
  margin-top: 12px;
}

.action-subtitle {
  font-size: 0.875rem;
  color: #666;
  margin-top: 4px;
}

/* Responsive adjustments */
@media (max-width: 768px) {
  .stat-value {
    font-size: 2rem;
  }
  
  .stat-icon-wrapper {
    width: 48px;
    height: 48px;
  }
  
  .stat-icon-wrapper :deep(.q-icon) {
    font-size: 24px;
  }
}

/* Dark mode support */
@media (prefers-color-scheme: dark) {
  .stat-value {
    color: #e0e0e0;
  }
  
  .stat-label {
    color: #999;
  }
  
  .detail-value {
    color: #e0e0e0;
  }
  
  .action-title {
    color: #e0e0e0;
  }
  
  .stat-details {
    background: rgba(255, 255, 255, 0.05);
  }
}
</style>
