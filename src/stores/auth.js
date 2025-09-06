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
        const response = await apiClient.post('auth/login', credentials);
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
        const response = await apiClient.get('auth/profile');
        this.user = response.data.user;
      } catch (e) {
        console.error('Failed to fetch profile:', e);
        this.logout(); // Logout if token is invalid
      }
    },
    async updateProfile(userData) {
      this.loading = true;
      this.error = null;
      try {
        // The API does not return the updated user object, so we don't expect a body.
        await apiClient.patch(`users/${userData.id}`, { name: userData.name });
        // Manually update the user in the store on success.
        if (this.user) {
          this.user.name = userData.name;
        }
      } catch (e) {
        this.error = 'Failed to update profile.';
        console.error('Profile update failed:', e);
      } finally {
        this.loading = false;
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