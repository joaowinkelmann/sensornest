import { ref } from 'vue';
import api from '@/api'; // Assuming you have an api module for making requests

export function useStatistics() {
  const statistics = ref(null);
  const loading = ref(false);
  const error = ref(null);

  const fetchStatistics = async (resourceType, id) => {
    loading.value = true;
    error.value = null;
    statistics.value = null;

    try {
      const response = await api.get(`/statistics/${resourceType}/${id}`);
      statistics.value = response.data;
    } catch (err) {
      error.value = err;
    } finally {
      loading.value = false;
    }
  };

  return {
    statistics,
    loading,
    error,
    fetchStatistics,
  };
}
