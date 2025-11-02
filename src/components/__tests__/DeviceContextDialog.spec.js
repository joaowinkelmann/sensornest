import { describe, it, expect, vi } from 'vitest';
import { mount } from '@vue/test-utils';
import { createI18n } from 'vue-i18n';
import flushPromises from 'flush-promises';
import { createTestingPinia } from '@pinia/testing';
import { Quasar } from 'quasar';
import DeviceContextDialog from '../DeviceContextDialog.vue';

const i18n = createI18n({
  legacy: false,
  locale: 'en-US',
  messages: {
    'en-US': {
      devices: {
        contextDialog: {
          // Updated: title no longer includes deviceName
          title: 'Device',
          statisticsTab: 'Statistics',
          // Match the current UI label
          zoneTab: 'Zone',
        },
      },
      buttons: {
        close: 'Close',
      },
    },
  },
});

vi.mock('@/composables/useStatistics', () => ({
  useStatistics: () => ({
    statistics: vi.fn(),
    loading: vi.fn(),
    error: vi.fn(),
    fetchDeviceGraph: vi.fn(),
  }),
}));

describe('DeviceContextDialog', () => {
  const mountComponent = (props) => {
    return mount(DeviceContextDialog, {
      global: {
        plugins: [
          i18n,
          Quasar,
          createTestingPinia({
            createSpy: vi.fn,
          }),
        ],
        stubs: {
          'q-dialog': {
            template: '<div><slot /></div>',
          },
          StatisticsChart: true,
          DeviceZoneDialog: true,
        },
      },
      props,
      attachTo: document.body,
    });
  };

  it('renders the dialog with the correct title', async () => {
    const wrapper = mountComponent({
      show: true,
      device: { id: '1', name: 'Test Device' },
    });
    await wrapper.vm.$nextTick();
    await flushPromises();
    // Updated expectation: generic title
    expect(wrapper.text()).toContain('Device');
  });

  it('emits a "close" event when the close button is clicked', async () => {
    const wrapper = mountComponent({
      show: true,
      device: { id: '1', name: 'Test Device' },
    });
    await flushPromises();
    const buttons = wrapper.findAllComponents({ name: 'QBtn' });
    const closeButton = buttons.find(b => b.props('label') === 'Close');
    await closeButton.trigger('click');
    expect(wrapper.emitted('close')).toHaveLength(1);
  });

  it('renders the dialog title and tabs', async () => {
    const wrapper = mountComponent({
      show: true,
      device: { id: 'dev-1', name: 'Test Device' }
    });

    await wrapper.vm.$nextTick();
    await flushPromises();

    expect(wrapper.text()).toContain('Device');
    expect(wrapper.text()).toContain('Statistics');
    expect(wrapper.text()).toContain('Zone');
  });
});
