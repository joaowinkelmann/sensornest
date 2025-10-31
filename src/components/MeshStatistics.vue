<template>
  <div class="q-pa-md">
    <div v-if="loading" class="flex items-center">
      <q-spinner-dots size="30px" class="q-mr-sm" />
      <span>{{ t('statistics.loading') }}</span>
    </div>
    <div v-else-if="error" class="text-negative">
      <q-icon name="error" />
      <span>{{ t('statistics.error') }}</span>
    </div>
    <div v-else-if="statistics" class="row q-col-gutter-md">
      <div v-for="period in Object.keys(statistics)" :key="period" class="col-12 col-md-4">
        <q-card>
          <q-card-section>
            <div class="text-h6">{{ t(`statistics.periods.${period}`) }}</div>
          </q-card-section>
          <q-card-section>
            <v-chart class="chart" :option="getChartOption(period)" autoresize />
          </q-card-section>
        </q-card>
      </div>
    </div>
  </div>
</template>

<script setup>
import { ref, watch, computed } from 'vue';
import { useStatistics } from '@/composables/useStatistics';
import { useI18n } from 'vue-i18n';
import { use } from 'echarts/core';
import { CanvasRenderer } from 'echarts/renderers';
import { BarChart } from 'echarts/charts';
import {
  TitleComponent,
  TooltipComponent,
  LegendComponent,
  GridComponent,
} from 'echarts/components';
import VChart, { THEME_KEY } from 'vue-echarts';

use([
  CanvasRenderer,
  BarChart,
  TitleComponent,
  TooltipComponent,
  LegendComponent,
  GridComponent,
]);

const props = defineProps({
  mesh: {
    type: Object,
    required: true,
  },
  expanded: {
    type: Boolean,
    default: false,
  },
});

const { t } = useI18n();
const { statistics, loading, error, fetchStatistics } = useStatistics();

watch(() => props.expanded, (newVal) => {
  if (newVal && !statistics.value) {
    fetchStatistics('mesh', props.mesh.id);
  }
}, { immediate: true });

const getChartOption = (period) => {
  const stats = statistics.value[period];
  return {
    tooltip: {
      trigger: 'axis',
      axisPointer: {
        type: 'shadow',
      },
    },
    legend: {
      data: [t('statistics.temperature'), t('statistics.humidity')],
    },
    grid: {
      left: '3%',
      right: '4%',
      bottom: '3%',
      containLabel: true,
    },
    xAxis: {
      type: 'category',
      data: [t('statistics.average'), t('statistics.highest')],
    },
    yAxis: {
      type: 'value',
    },
    series: [
      {
        name: t('statistics.temperature'),
        type: 'bar',
        data: [stats.averageTemperature, stats.highestTemperature],
      },
      {
        name: t('statistics.humidity'),
        type: 'bar',
        data: [stats.averageHumidity, stats.highestHumidity],
      },
    ],
  };
};
</script>

<style scoped>
.chart {
  height: 300px;
}
.text-negative {
  color: #db4437;
}
</style>
