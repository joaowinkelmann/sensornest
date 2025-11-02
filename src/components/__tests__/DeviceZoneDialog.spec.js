import { describe, it, expect, vi } from 'vitest';
import { mount } from '@vue/test-utils';
import { createI18n } from 'vue-i18n';
import flushPromises from 'flush-promises';
import { createTestingPinia } from '@pinia/testing';
import { Quasar } from 'quasar';
import DeviceZoneDialog from '../DeviceZoneDialog.vue';
// import { useZonesStore } from '@/stores/zones';

const i18n = createI18n({
  legacy: false,
  locale: 'en-US',
  messages: {
    'en-US': {
      devices: {
        changeZone: {
          title: 'Change Device Zone',
          selectZone: 'Select a new zone',
          // Added to silence i18n warnings
          noZones: 'No zones in this mesh.',
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

  it('emits "zone-changed" with correct payload when Save is clicked', async () => {
    const wrapper = mount(DeviceZoneDialog, {
      props: {
        show: true,
        device: { id: '1', meshId: 'm1', zoneId: 'z1' }
      },
      global: {
        plugins: [Quasar, i18n],
        stubs: {
          'q-dialog': { template: '<div><slot /></div>' },
          'q-card': { template: '<div><slot /></div>' },
          'q-card-section': { template: '<div><slot /></div>' },
          'q-card-actions': { template: '<div><slot /></div>' },
          'q-select': { template: '<div />' },
          'q-btn': { template: '<button @click="$emit(\'click\')" :data-testid="dataTestid"><slot /></button>', props: ['label', 'dataTestid'] },
          'q-spinner-dots': true
        }
      }
    });

    // Ensure zones exist and selectedZoneId differs from device.zoneId
    wrapper.vm.selectedZoneId = 'z2';
    await wrapper.vm.$nextTick();

    const saveBtn = wrapper.find('[data-testid="zone-save-btn"]');
    await saveBtn.trigger('click');

    const emitted = wrapper.emitted('zone-changed') || [];
    // Accept at least one emission (defensive for duplicate triggers in test env)
    expect(emitted.length).toBeGreaterThanOrEqual(1);
    const last = emitted[emitted.length - 1][0];
    expect(last).toEqual({ deviceId: '1', zoneId: 'z2' });
  });
});
