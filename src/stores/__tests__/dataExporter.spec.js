import { describe, it, expect, vi, beforeEach } from 'vitest';
import { createPinia, setActivePinia } from 'pinia';
import { useDataExporterStore } from '../dataExporter';
import apiClient from '@/api';

vi.mock('@/api');

describe('store dataExporter', () => {
  let store;

  beforeEach(() => {
    setActivePinia(createPinia());
    store = useDataExporterStore();
  });

  it('chama o endpoint de exportação com o payload correto', async () => {
    apiClient.post.mockResolvedValue({ data: 'http://localhost/export.csv' });

    await store.exportData('mesh', 'mesh1');

    expect(apiClient.post).toHaveBeenCalledWith('/data-exporter/export', {
      entityType: 'mesh',
      entityId: 'mesh1',
      maxBytesPerFile: 5 * 1024 * 1024, // 5 MiB
    });
  });

  it('retorna a URL da chamada da API', async () => {
    apiClient.post.mockResolvedValue({ data: 'http://localhost/export.csv' });

    const url = await store.exportData('mesh', 'mesh1');

    expect(url).toBe('http://localhost/export.csv');
  });

  it('define estados de carregamento e erro corretamente', async () => {
    apiClient.post.mockRejectedValue(new Error('Network Error'));

    await expect(store.exportData('mesh', 'mesh1')).rejects.toThrow('Network Error');
    expect(store.loading).toBe(false);
    expect(store.error).toBe('Falha ao exportar dados.');
  });
});
