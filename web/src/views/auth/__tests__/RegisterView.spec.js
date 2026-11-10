import { describe, it, expect, vi, beforeEach } from 'vitest';
import { mount } from '@vue/test-utils';
import { createPinia, setActivePinia } from 'pinia';
import { createI18n } from 'vue-i18n';
import { Quasar } from 'quasar';
import RegisterView from '../RegisterView.vue';
// import apiClient from '@/api';
import flushPromises from 'flush-promises';

const i18n = createI18n({
  legacy: false, // composition API
  locale: 'en',
  messages: {
    en: {
      register: {
        title: 'Register',
        nameLabel: 'Name',
        emailLabel: 'Email',
        passwordLabel: 'Password',
        confirmPasswordLabel: 'Confirm Password',
        registerButton: 'Register',
        loginButton: 'Login',
        nameRequired: 'Name is required',
        emailRequired: 'Email is required',
        passwordRequired: 'Password is required',
        confirmPasswordRequired: 'Confirm password is required',
        passwordsMustMatch: 'Passwords must match',
        successMessage: 'Registration successful!',
        errorMessage: 'Registration failed.'
      }
    }
  }
});

vi.mock('vue-router', () => ({
  useRouter: () => ({
    push: vi.fn()
  })
}));

let postMock;
vi.mock('@/api', () => ({
  default: {
    post: (...args) => postMock(...args)
  }
}));

describe('RegisterView.vue', () => {
  beforeEach(() => {
    setActivePinia(createPinia());
    postMock = vi.fn();
  });

  const mountComponent = () => mount(RegisterView, {
    global: {
      plugins: [i18n, Quasar],
      stubs: {
        'q-layout': { template: '<div><slot /></div>' },
        'q-page-container': { template: '<div><slot /></div>' },
        'q-page': { template: '<div><slot /></div>' }
      }
    }
  });

  it('renders the register form', () => {
    const wrapper = mountComponent();

    expect(wrapper.text()).toContain('Register');
    expect(wrapper.find('input[type="email"]').exists()).toBe(true);
    expect(wrapper.findAll('input[type="password"]').length).toBe(2);
    expect(wrapper.find('button[type="submit"]').exists()).toBe(true);
    expect(wrapper.text()).toContain('Login');
  });

  it('calls the register endpoint when the form is submitted', async () => {
    const wrapper = mountComponent();

    await wrapper.find('input[aria-label="Name"]').setValue('Test User');
    await wrapper.find('input[type="email"]').setValue('test@example.com');
    await wrapper.find('input[aria-label="Password"]').setValue('password');
    await wrapper.find('input[aria-label="Confirm Password"]').setValue('password');
    await wrapper.find('form').trigger('submit.prevent');
    await flushPromises();

    expect(postMock).toHaveBeenCalledWith('/users/register', {
      name: 'Test User',
      email: 'test@example.com',
      password: 'password'
    });
  });
});
