import { describe, it, expect, vi } from 'vitest';
import { mount } from '@vue/test-utils';
import { Quasar } from 'quasar';
import MainSidebar from '../MainSidebar.vue';
import { h } from 'vue';

vi.mock('vue-i18n', () => ({
  useI18n: () => ({
    t: (key) => key,
  }),
}));

import { QLayout } from 'quasar';

describe('MainSidebar.vue', () => {
  it('renders all navigation links', () => {
    const wrapper = mount(QLayout, {
      slots: {
        default: h(MainSidebar, { modelValue: true, user: { name: 'Test User' } }),
      },
      global: {
        plugins: [Quasar],
        stubs: ['router-link'],
      },
    });

    const links = wrapper.findAllComponents({ name: 'QItem' });
    // expect(links).toHaveLength(9);
    expect(links).toHaveLength(8);
    expect(links[1].text()).toContain('sidebar.home');
    expect(links[2].text()).toContain('sidebar.meshes');
    expect(links[3].text()).toContain('sidebar.devices');
    // expect(links[4].text()).toContain('sidebar.reports');
    expect(links[4].text()).toContain('sidebar.data');
    expect(links[5].text()).toContain('sidebar.mapEditor');
    expect(links[6].text()).toContain('Test User');
    expect(links[7].text()).toContain('Logout');
  });
});
