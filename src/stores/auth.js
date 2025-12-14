import { defineStore } from 'pinia';
import apiClient from '@/api';
import router from '@/router';
import { Notify } from 'quasar';

export const useAuthStore = defineStore('auth', {
  state: () => ({
    token: localStorage.getItem('token') || null,
    user: null,
    loading: false,
    error: null,
    // flags de cache
    profileLoaded: false,
    profileLoadedAt: null,
    _profilePromise: null,
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
        await this.getProfile();
        router.push('/');
      } catch (e) {
        this.error = 'Invalid email or password.';
        console.error('Login failed:', e);
      } finally {
        this.loading = false;
      }
    },
    async getProfile() {
      this.loading = true;
      this.error = null;
      try {
        const response = await apiClient.get('auth/profile');
        this.user = response.data.user || response.data;
      } catch (e) {
        this.error = 'Failed to fetch profile.';
        console.error('Profile fetch failed:', e);
      } finally {
        this.loading = false;
      }
    },
    async getProfileIfNeeded({ force = false, maxAgeMs = 5 * 60 * 1000 } = {}) {
      if (this._profilePromise) {
        return this._profilePromise;
      }

      const now = Date.now();
      const isStale = this.profileLoadedAt && (now - this.profileLoadedAt) > maxAgeMs;

      if (!force && this.profileLoaded && !isStale && this.user) {
        return Promise.resolve();
      }

      this._profilePromise = (async () => {
        try {
          const response = await apiClient.get('auth/profile');
          this.user = response.data.user || response.data;
          this.profileLoaded = true;
          this.profileLoadedAt = Date.now();
        } catch (e) {
          this.error = 'Failed to fetch profile.';
          console.error('Profile fetch failed:', e);
        } finally {
          this._profilePromise = null;
        }
      })();

      return this._profilePromise;
    },
    async updateProfile({ id, name }) {
      this.loading = true;
      this.error = null;
      try {
        const response = await apiClient.patch(`users/${id}`, { name });
        this.user = response.data.user || response.data;

        Notify.create({
          type: 'positive',
          message: response.data.message || 'Usuário atualizado com sucesso',
          position: 'top',
          timeout: 2000
        });

        return response.data;
      } catch (e) {
        this.error = 'Failed to update profile.';
        console.error('Profile update failed:', e);

        Notify.create({
          type: 'negative',
          message: 'Falha ao atualizar perfil',
          position: 'top',
          timeout: 2000
        });

        throw e;
      } finally {
        this.loading = false;
      }
    },
    logout() {
      this.token = null;
      this.user = null;
      localStorage.removeItem('token');
      router.push('/login');
      // reseta flags de cache
      this.profileLoaded = false;
      this.profileLoadedAt = null;
    },
  },
});