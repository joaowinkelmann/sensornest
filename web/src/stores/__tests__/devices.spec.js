import { describe, it, expect, beforeEach, vi } from 'vitest'
import { setActivePinia, createPinia } from 'pinia'

vi.mock('@/api', () => {
  const get = vi.fn()
  const post = vi.fn()
  const patch = vi.fn()
  return { default: { get, post, patch } }
})

import apiClient from '@/api'
import { useDevicesStore } from '../devices'

describe('store de dispositivos: seleção de URL fetchDevices e tratamento de erros', () => {
  beforeEach(() => {
    setActivePinia(createPinia())
    apiClient.get.mockReset()
  })

  it('chama /devices quando nenhum filtro é fornecido', async () => {
    const store = useDevicesStore()
    apiClient.get.mockResolvedValueOnce({ data: [{ id: 'd1' }] })

    await store.fetchDevices()

    expect(apiClient.get).toHaveBeenCalledTimes(1)
    expect(apiClient.get.mock.calls[0][0]).toBe('/devices')
    expect(store.devices).toEqual([{ id: 'd1' }])
    expect(store.error).toBeNull()
  })

  it('chama /meshes/{meshId}/devices quando meshId é fornecido', async () => {
    const store = useDevicesStore()
    apiClient.get.mockResolvedValueOnce({ data: [{ id: 'd2' }] })

    await store.fetchDevices({ meshId: 'mesh1' })

    expect(apiClient.get).toHaveBeenCalledTimes(1)
    expect(apiClient.get.mock.calls[0][0]).toBe('/meshes/mesh1/devices')
    expect(store.devices).toEqual([{ id: 'd2' }])
  })

  it('chama /zones/{zoneId}/devices quando zoneId é fornecido (sobrescreve rota da malha)', async () => {
    const store = useDevicesStore()
    apiClient.get.mockResolvedValueOnce({ data: [{ id: 'd3' }] })

    await store.fetchDevices({ meshId: 'meshX', zoneId: 'zone1' })

    expect(apiClient.get).toHaveBeenCalledTimes(1)
    expect(apiClient.get.mock.calls[0][0]).toBe('/zones/zone1/devices')
    expect(store.devices).toEqual([{ id: 'd3' }])
  })

  it('trata erros limpando dispositivos e definindo mensagem de erro', async () => {
    const store = useDevicesStore()
    apiClient.get.mockRejectedValueOnce(new Error('network'))

    await store.fetchDevices()

    expect(store.devices).toEqual([])
    expect(store.error).toBe('Falha ao buscar dispositivos.')
  })

  it('clearDevices limpa a lista', async () => {
    const store = useDevicesStore()
    store.devices = [{ id: 'dX' }]

    store.clearDevices()

    expect(store.devices).toEqual([])
  })
})