import { describe, it, expect, vi } from 'vitest';
import { mount } from '@vue/test-utils';
import { Quasar } from 'quasar';
import ZoneStatistics from '../ZoneStatistics.vue';
// import { useZonesStore } from '@/stores/zones';
// import { useStatistics } from '@/composables/useStatistics';

const mockFetchStatistics = vi.fn();

vi.mock('@/stores/zones', () => ({
  useZonesStore: vi.fn(() => ({
    zones: [
      { id: 'zone1', name: 'Zone 1', meshId: 'mesh1' },
      { id: 'zone2', name: 'Zone 2', meshId: 'mesh1' },
    ],
    fetchAllZones: vi.fn(),
  })),
}));

vi.mock('@/composables/useStatistics', () => ({
  useStatistics: vi.fn(() => ({
    statistics: null,
    loading: false,
    error: null,
    fetchStatistics: mockFetchStatistics,
  })),
}));

describe('ZoneStatistics.vue', () => {
  it('renders the component and fetches statistics on button click', async () => {
    const wrapper = mount(ZoneStatistics, {
      props: {
        meshId: 'mesh1',
      },
      global: {
        plugins: [Quasar],
      },
    });

    expect(wrapper.findAll('.q-item')).toHaveLength(2);

    await wrapper.find('button[aria-label="Show statistics"]').trigger('click');
    expect(mockFetchStatistics).toHaveBeenCalledWith('zone', 'zone1');
  });
});
