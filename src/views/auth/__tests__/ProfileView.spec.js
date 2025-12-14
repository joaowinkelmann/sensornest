import { describe, it, expect, vi, beforeEach } from 'vitest';
import { mount } from '@vue/test-utils';
import { createPinia, setActivePinia } from 'pinia';
import { createI18n } from 'vue-i18n';
import { Quasar } from 'quasar';
import ProfileView from '../ProfileView.vue';
// import { useAuthStore } from '@/stores/auth';
import { ref } from 'vue';
import flushPromises from 'flush-promises';

const i18n = createI18n({
  legacy: false, // composition api
  locale: 'en',
  messages: {
    en: {
      profile: {
        title: 'Profile',
        nameLabel: 'Name',
        emailLabel: 'Email',
        saveButton: 'Save',
        nameRequired: 'Name is required',
        loading: 'Loading...'
      }
    }
  }
});

let getProfileIfNeededMock;
let updateProfileMock;
let userRef;

vi.mock('@/stores/auth', () => ({
  useAuthStore: vi.fn(() => ({
    user: userRef,
    getProfileIfNeeded: getProfileIfNeededMock,
    updateProfile: updateProfileMock
  }))
}));

describe('ProfileView.vue', () => {
  beforeEach(() => {
    setActivePinia(createPinia());
    getProfileIfNeededMock = vi.fn();
    updateProfileMock = vi.fn();
    userRef = ref({
      id: 1,
      name: 'Test User',
      email: 'test@example.com'
    });
  });

  const mountComponent = () => mount(ProfileView, {
    global: {
      plugins: [i18n, Quasar],
      stubs: {
        'q-layout': { template: '<div><slot /></div>' },
        'q-page-container': { template: '<div><slot /></div>' },
        'q-page': { template: '<div><slot /></div>' }
      }
    }
  });

  it('renders the profile form with user data', async () => {
    const wrapper = mountComponent();
    await flushPromises();

    expect(wrapper.text()).toContain('Profile');
    expect(wrapper.find('input[aria-label="Name"]').element.value).toBe('Test User');
    expect(wrapper.find('input[aria-label="Email"]').element.value).toBe('test@example.com');
  });

  it('calls the updateProfile action when the form is submitted', async () => {
    const wrapper = mountComponent();
    await flushPromises();

    await wrapper.find('input[aria-label="Name"]').setValue('Updated User');
    await wrapper.find('form').trigger('submit.prevent');
    await flushPromises();

    expect(updateProfileMock).toHaveBeenCalledWith({
      id: 1,
      name: 'Updated User'
    });
  });
});
