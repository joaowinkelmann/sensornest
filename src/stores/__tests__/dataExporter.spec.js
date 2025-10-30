import { describe, it, expect, vi, beforeEach } from 'vitest';
import { createPinia, setActivePinia } from 'pinia';
import { useDataExporterStore } from '../dataExporter';
import apiClient from '@/api';

vi.mock('@/api');

describe('dataExporter store', () => {
  let store;

  beforeEach(() => {
    setActivePinia(createPinia());
    store = useDataExporterStore();
  });

  it('calls the export endpoint with the correct payload', async () => {
    apiClient.post.mockResolvedValue({ data: 'http://localhost/export.csv' });

    await store.exportData('mesh', 'mesh1');

    expect(apiClient.post).toHaveBeenCalledWith('/data-exporter/export', {
      entityType: 'mesh',
      entityId: 'mesh1',
      maxBytesPerFile: 5 * 1024 * 1024, // 5 MiB
    });
  });

  it('returns the URL from the API call', async () => {
    apiClient.post.mockResolvedValue({ data: 'http://localhost/export.csv' });

    const url = await store.exportData('mesh', 'mesh1');

    expect(url).toBe('http://localhost/export.csv');
  });

  it('sets loading and error states correctly', async () => {
    apiClient.post.mockRejectedValue(new Error('Network Error'));

    await expect(store.exportData('mesh', 'mesh1')).rejects.toThrow('Network Error');
    expect(store.loading).toBe(false);
    expect(store.error).toBe('Failed to export data.');
  });
});
