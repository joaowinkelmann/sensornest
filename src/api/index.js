import axios from 'axios';
import { useAuthStore } from '@/stores/auth';

// Use Vite's import.meta.env for environment variables
const apiClient = axios.create({
  baseURL: import.meta.env.VITE_API_URL || 'http://localhost:1234',
  headers: {
    'Content-Type': 'application/json',
  },
});

// Interceptor to add the auth token to every request
apiClient.interceptors.request.use((config) => {
  const authStore = useAuthStore();
  const token = authStore.token;
  if (token) {
    config.headers.Authorization = `Bearer ${token}`;
  }
  return config;
});

export default apiClient;