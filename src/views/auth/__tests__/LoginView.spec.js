import { describe, it, expect, vi, beforeEach } from 'vitest';
import { mount } from '@vue/test-utils';
import { createPinia, setActivePinia } from 'pinia';
import { createI18n } from 'vue-i18n';
import { Quasar, Notify } from 'quasar';
import LoginView from '../LoginView.vue';
// import { useAuthStore } from '@/stores/auth';
import flushPromises from 'flush-promises';

const i18n = createI18n({
  legacy: false,
  locale: 'en',
  messages: {
    en: {
      login: {
        title: 'Login',
        emailLabel: 'Email',
        passwordLabel: 'Password',
        signInButton: 'Sign In',
        registerButton: 'Register',
        emailRequired: 'Email is required',
        passwordRequired: 'Password is required'
      }
    }
  }
});

const queryRef = vi.hoisted(() => ({ value: {} }));

vi.mock('vue-router', () => ({
  useRouter: () => ({
    push: vi.fn(),
    replace: () => { }
  }),
  useRoute: () => ({
    query: queryRef.value
  }),
  createRouter: vi.fn(() => ({
    beforeEach: vi.fn(),
  })),
  createWebHistory: vi.fn(),
}));

import { useRouter, useRoute } from 'vue-router';
import apiClient from '@/api';

vi.mock('@/api', () => ({
  default: {
    get: vi.fn(),
    post: vi.fn()
  }
}));

let loginMock;
vi.mock('@/stores/auth', () => ({
  useAuthStore: vi.fn(() => ({
    login: loginMock,
    error: null,
    loading: false
  }))
}));

describe('LoginView.vue', () => {
  // const pushMock = vi.fn(); // Unused
  // const replaceMock = vi.fn(); // Unused

  beforeEach(() => {
    setActivePinia(createPinia());
    loginMock = vi.fn();

    // Reset query
    queryRef.value = {};



    apiClient.get.mockClear();
    apiClient.post.mockClear();
  });

  const mountComponent = () => mount(LoginView, {
    global: {
      plugins: [i18n, [Quasar, { plugins: { Notify } }]],
      stubs: {
        'q-layout': { template: '<div><slot /></div>' },
        'q-page-container': { template: '<div><slot /></div>' },
        'q-page': { template: '<div><slot /></div>' },
        // Stub dialogs to avoid intense rendering but keep existence check
        'PasswordRecoveryDialog': true,
        'ResetPasswordDialog': true
      }
    }
  });

  it('renders the login form', () => {
    const wrapper = mountComponent();
    expect(wrapper.text()).toContain('Login');
    expect(wrapper.find('input[type="email"]').exists()).toBe(true);
    expect(wrapper.find('input[type="password"]').exists()).toBe(true);
    expect(wrapper.find('button[type="submit"]').exists()).toBe(true);
    expect(wrapper.text()).toContain('Register');
  });

  it('calls the login action when the form is submitted', async () => {
    const wrapper = mountComponent();

    await wrapper.find('input[type="email"]').setValue('test@example.com');
    await wrapper.find('input[type="password"]').setValue('password');
    await wrapper.find('form').trigger('submit.prevent');
    await flushPromises();

    expect(loginMock).toHaveBeenCalledWith({
      email: 'test@example.com',
      password: 'password'
    });
  });

  it('checks for reset token on mount', async () => {
    queryRef.value = { token: 'abc-123' };
    apiClient.get.mockResolvedValue({ data: { valid: true } });

    mountComponent();

    expect(apiClient.get).toHaveBeenCalledWith('/users/reset-password/check', { params: { token: 'abc-123' } });
  });

  it('shows reset dialog if token is valid', async () => {
    queryRef.value = { token: 'valid-token' };
    apiClient.get.mockResolvedValue({ data: { valid: true } });

    const wrapper = mountComponent();
    await flushPromises();

    const resetDialog = wrapper.findComponent({ name: 'ResetPasswordDialog' });
    expect(resetDialog.exists()).toBe(true);
    expect(resetDialog.props('show')).toBe(true);
    expect(resetDialog.props('token')).toBe('valid-token');
  });

  it('does not show reset dialog if token is invalid', async () => {
    queryRef.value = { token: 'invalid-token' };
    apiClient.get.mockResolvedValue({ data: { valid: false } });

    const wrapper = mountComponent();
    await flushPromises();

    const resetDialog = wrapper.findComponent({ name: 'ResetPasswordDialog' });
    expect(resetDialog.exists()).toBe(true);
    expect(resetDialog.props('show')).toBe(false);
  });
});
