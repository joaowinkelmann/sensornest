import { describe, it, expect, vi, beforeEach } from 'vitest';
import { mount } from '@vue/test-utils';
import { createPinia, setActivePinia } from 'pinia';
import { createI18n } from 'vue-i18n';
import { Quasar } from 'quasar';
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

vi.mock('vue-router', () => ({
  useRouter: () => ({
    push: vi.fn()
  }),
  createRouter: vi.fn(() => ({
    beforeEach: vi.fn(),
  })),
  createWebHistory: vi.fn(),
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
  beforeEach(() => {
    setActivePinia(createPinia());
    loginMock = vi.fn();
  });

  const mountComponent = () => mount(LoginView, {
    global: {
      plugins: [i18n, Quasar],
      stubs: {
        'q-layout': { template: '<div><slot /></div>' },
        'q-page-container': { template: '<div><slot /></div>' },
        'q-page': { template: '<div><slot /></div>' }
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
});
