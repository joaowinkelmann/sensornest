import { describe, it, expect, vi } from 'vitest';
import { mount } from '@vue/test-utils';
import { Quasar } from 'quasar';
import DeviceManager from '../DeviceManager.vue';
import { createI18n } from 'vue-i18n';
import flushPromises from 'flush-promises';

const i18n = createI18n({
  legacy: false,
  locale: 'en-US',
  messages: {
    'en-US': {
      devices: {
        title: 'Device Manager',
        filterByMesh: 'Filter by Mesh',
        filterByZone: 'Filter by Zone',
      },
    },
  },
});

const mockFetchDevices = vi.fn();
const mockFetchMeshes = vi.fn();
const mockFetchAllZones = vi.fn();

vi.mock('@/stores/devices', () => ({
  useDevicesStore: vi.fn(() => ({
    devices: [
      { id: 'device1', name: 'Device 1', macAddress: '00:00:00:00:00:01' },
      { id: 'device2', name: 'Device 2', macAddress: '00:00:00:00:00:02' },
    ],
    fetchDevices: mockFetchDevices,
  })),
}));

vi.mock('@/stores/meshes', () => ({
  useMeshesStore: vi.fn(() => ({
    meshes: [
      { id: 'mesh1', name: 'Mesh 1' },
      { id: 'mesh2', name: 'Mesh 2' },
    ],
    fetchMeshes: mockFetchMeshes,
  })),
}));

vi.mock('@/stores/zones', () => ({
  useZonesStore: vi.fn(() => ({
    zones: [
      { id: 'zone1', name: 'Zone 1', meshId: 'mesh1' },
      { id: 'zone2', name: 'Zone 2', meshId: 'mesh1' },
    ],
    fetchAllZones: mockFetchAllZones,
  })),
}));

vi.mock('@/composables/useStatistics', () => ({
  useStatistics: vi.fn(() => ({
    statistics: null,
    loading: false,
    error: null,
    fetchStatistics: vi.fn(),
  })),
}));

describe('DeviceManager.vue', () => {
  it('renders the component and fetches initial data', async () => {
    const wrapper = mount(DeviceManager, {
      global: {
        plugins: [Quasar, i18n],
      },
    });

    await flushPromises();

    expect(wrapper.find('.text-h4').text()).toBe('Device Manager');
    expect(mockFetchMeshes).toHaveBeenCalled();
    expect(mockFetchDevices).toHaveBeenCalled();
    expect(wrapper.findAll('.q-item')).toHaveLength(2);
  });
});
