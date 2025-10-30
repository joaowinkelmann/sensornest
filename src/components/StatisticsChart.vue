<template>
  <div>
    <v-chart class="chart" :option="chartOption" autoresize />
  </div>
</template>

<script setup>
import { ref, watch } from 'vue';
import { use } from 'echarts/core';
import { CanvasRenderer } from 'echarts/renderers';
import { LineChart } from 'echarts/charts';
import {
  TitleComponent,
  TooltipComponent,
  LegendComponent,
  GridComponent,
  ToolboxComponent,
} from 'echarts/components';
import VChart from 'vue-echarts';

use([
  CanvasRenderer,
  LineChart,
  TitleComponent,
  TooltipComponent,
  LegendComponent,
  GridComponent,
  ToolboxComponent,
]);

const props = defineProps({
  chartData: {
    type: Object,
    required: true,
  },
  period: {
    type: String,
    required: true,
  },
});

const chartOption = ref({});

const generateChartOption = (data, period) => {
  if (!data || !data[period]) {
    return {};
  }

  const seriesData = data[period];

  return {
    tooltip: {
      trigger: 'axis',
    },
    legend: {
      data: ['Temperature', 'Humidity'],
    },
    grid: {
      left: '3%',
      right: '4%',
      bottom: '3%',
      containLabel: true,
    },
    toolbox: {
      feature: {
        saveAsImage: {},
      },
    },
    xAxis: {
      type: 'category',
      boundaryGap: false,
      data: seriesData.map(item => item.bucket),
    },
    yAxis: {
      type: 'value',
    },
    series: [
      {
        name: 'Temperature',
        type: 'line',
        data: seriesData.map(item => item.avgTemperature),
      },
      {
        name: 'Humidity',
        type: 'line',
        data: seriesData.map(item => item.avgHumidity),
      },
    ],
  };
};

watch(
  () => [props.chartData, props.period],
  ([newData, newPeriod]) => {
    chartOption.value = generateChartOption(newData, newPeriod);
  },
  { immediate: true }
);
</script>

<style scoped>
.chart {
  height: 400px;
}
</style>
