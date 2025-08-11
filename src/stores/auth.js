import { defineStore } from 'pinia';
import apiClient from '@/api';
import router from '@/router';

export const useAuthStore = defineStore('auth', {
  state: () => ({
    token: localStorage.getItem('token') || null,
    user: null,
    loading: false,
    error: null,
  }),
  getters: {
    isAuthenticated: (state) => !!state.token,
  },
  actions: {
    async login(credentials) {
      this.loading = true;
      this.error = null;
      try {
        const response = await apiClient.post('/auth/login/', credentials);
        this.token = response.data.access_token;
        localStorage.setItem('token', this.token);
        await this.getProfile(); // Fetch user profile after login
        router.push('/'); // Redirect to dashboard
      } catch (e) {
        this.error = 'Invalid email or password.';
        console.error('Login failed:', e);
      } finally {
        this.loading = false;
      }
    },
    async getProfile() {
      if (!this.token) return;
      try {
        // This corresponds to your GET /auth/profile endpoint
        const response = await apiClient.get('/auth/profile');
        this.user = response.data;
      } catch (e) {
        console.error('Failed to fetch profile:', e);
        this.logout(); // Logout if token is invalid
      }
    },
    logout() {
      this.token = null;
      this.user = null;
      localStorage.removeItem('token');
      router.push('/login');
    },
  },
});