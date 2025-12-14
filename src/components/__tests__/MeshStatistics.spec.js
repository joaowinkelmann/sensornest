import { mount } from '@vue/test-utils';
import MeshStatistics from '../MeshStatistics.vue';
import { describe, it, expect, vi, beforeEach } from 'vitest';
import { ref } from 'vue';
// import { useI18n } from 'vue-i18n';
import * as useStatistics from '@/composables/useStatistics';
import flushPromises from 'flush-promises';

vi.mock('vue-echarts', () => ({
  default: {
    name: 'VChart',
    template: '<div class="stub-v-chart" />',
    props: ['option'],
  }
}));

vi.mock('@/composables/useStatistics');
vi.mock('vue-i18n', () => ({
  useI18n: () => ({
    t: (key) => key,
  }),
}));

const mockStatistics = {
  day: { averageTemperature: 20, highestTemperature: 30, averageHumidity: 50, highestHumidity: 60 },
  week: { averageTemperature: 18, highestTemperature: 28, averageHumidity: 48, highestHumidity: 58 },
  month: { averageTemperature: 15, highestTemperature: 25, averageHumidity: 45, highestHumidity: 55 },
  year: { averageTemperature: 12, highestTemperature: 22, averageHumidity: 42, highestHumidity: 52 },
};

describe('MeshStatistics.vue', () => {
  let useStatisticsMock;

  beforeEach(() => {
    vi.clearAllMocks();
    useStatisticsMock = {
      statistics: ref(null),
      loading: ref(false),
      error: ref(null),
      fetchStatistics: vi.fn(),
    };
    useStatistics.useStatistics.mockReturnValue(useStatisticsMock);
  });

  const mesh = { id: 'mesh-1', name: 'Test Mesh' };

  const mountComponent = (props = {}) => {
    return mount(MeshStatistics, {
      props: { mesh, ...props },
      global: {
        stubs: {
          'q-spinner-dots': true,
          'q-icon': true,
          'q-card': { template: '<div><slot /></div>' },
          'q-card-section': { template: '<div><slot /></div>' },
        },
      },
    });
  };

  it('renders loading state correctly', () => {
    useStatisticsMock.loading.value = true;
    const wrapper = mountComponent({ expanded: true });
    expect(wrapper.findComponent({ name: 'QSpinnerDots' }).exists()).toBe(true);
    expect(wrapper.text()).toContain('statistics.loading');
  });

  it('renders error state correctly', () => {
    useStatisticsMock.error.value = new Error('Failed to fetch');
    const wrapper = mountComponent({ expanded: true });
    expect(wrapper.findComponent({ name: 'QIcon' }).exists()).toBe(true);
    expect(wrapper.text()).toContain('statistics.error');
  });

  it('renders statistics and charts on success', async () => {
    useStatisticsMock.statistics.value = mockStatistics;
    const wrapper = mountComponent({ expanded: true });
    await flushPromises();
    expect(wrapper.findAll('.stub-v-chart').length).toBe(Object.keys(mockStatistics).length);
    const chart = wrapper.findComponent({ name: 'VChart' });
    expect(chart.props('option')).toBeDefined();
  });

  it('calls fetchStatistics when expanded', async () => {
    const wrapper = mountComponent({ expanded: false });
    await wrapper.setProps({ expanded: true });
    expect(useStatisticsMock.fetchStatistics).toHaveBeenCalledWith('mesh', 'mesh-1');
  });
});
