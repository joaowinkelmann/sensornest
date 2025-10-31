import { describe, it, expect, vi } from 'vitest';
import { mount } from '@vue/test-utils';
import { createI18n } from 'vue-i18n';
import flushPromises from 'flush-promises';
import { createTestingPinia } from '@pinia/testing';
import { Quasar } from 'quasar';
import DeviceZoneDialog from '../DeviceZoneDialog.vue';
import { useZonesStore } from '@/stores/zones';

const i18n = createI18n({
  legacy: false,
  locale: 'en-US',
  messages: {
    'en-US': {
      devices: {
        changeZone: {
          title: 'Change Device Zone',
          selectZone: 'Select a new zone',
        },
      },
      buttons: {
        cancel: 'Cancel',
        save: 'Save',
      },
    },
  },
});

describe('DeviceZoneDialog', () => {
  const mountComponent = (props) => {
    return mount(DeviceZoneDialog, {
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
        },
      },
      props,
      attachTo: document.body,
    });
  };

  it('renders the dialog with the correct title', async () => {
    const wrapper = mountComponent({
      show: true,
      device: { id: '1', name: 'Test Device', meshId: 'mesh1', zoneId: 'zone1' },
    });
    await wrapper.vm.$nextTick();
    await flushPromises();
    expect(wrapper.text()).toContain('Change Device Zone');
  });

  it('emits a "close" event when the cancel button is clicked', async () => {
    const wrapper = mountComponent({
      show: true,
      device: { id: '1', name: 'Test Device', meshId: 'mesh1', zoneId: 'zone1' },
    });
    await flushPromises();
    const buttons = wrapper.findAllComponents({ name: 'QBtn' });
    const cancelButton = buttons.find(b => b.props('label') === 'Cancel');
    await cancelButton.trigger('click');
    expect(wrapper.emitted('close')).toHaveLength(1);
  });

  it('emits a "zone-changed" event with the correct payload when the save button is clicked', async () => {
    const wrapper = mountComponent({
      show: true,
      device: { id: '1', name: 'Test Device', meshId: 'mesh1', zoneId: 'zone1' },
    });
    const zonesStore = useZonesStore();
    zonesStore.allZones = [
      { id: 'zone1', name: 'Zone 1', meshId: 'mesh1' },
      { id: 'zone2', name: 'Zone 2', meshId: 'mesh1' },
    ];
    await flushPromises();
    wrapper.vm.selectedZoneId = 'zone2';
    const buttons = wrapper.findAllComponents({ name: 'QBtn' });
    const saveButton = buttons.find(b => b.props('label') === 'Save');
    await saveButton.trigger('click');
    expect(wrapper.emitted('zone-changed')).toHaveLength(1);
    expect(wrapper.emitted('zone-changed')[0][0]).toEqual({
      deviceId: '1',
      zoneId: 'zone2',
    });
  });
});
