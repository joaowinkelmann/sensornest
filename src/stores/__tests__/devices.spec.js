import { describe, it, expect, beforeEach, vi } from 'vitest'
import { setActivePinia, createPinia } from 'pinia'

// Mock API client safely (no top-level mutated refs)
vi.mock('@/api', () => {
  const get = vi.fn()
  const post = vi.fn()
  const patch = vi.fn()
  return { default: { get, post, patch } }
})

import apiClient from '@/api'
import { useDevicesStore } from '../devices'

describe('devices store: fetchDevices URL selection and error handling', () => {
  beforeEach(() => {
    setActivePinia(createPinia())
    apiClient.get.mockReset()
  })

  it('calls /devices when no filters are provided', async () => {
    const store = useDevicesStore()
    apiClient.get.mockResolvedValueOnce({ data: [{ id: 'd1' }] })

    await store.fetchDevices()

    expect(apiClient.get).toHaveBeenCalledTimes(1)
    expect(apiClient.get.mock.calls[0][0]).toBe('/devices')
    expect(store.devices).toEqual([{ id: 'd1' }])
    expect(store.error).toBeNull()
  })

  it('calls /meshes/{meshId}/devices when meshId is provided', async () => {
    const store = useDevicesStore()
    apiClient.get.mockResolvedValueOnce({ data: [{ id: 'd2' }] })

    await store.fetchDevices({ meshId: 'mesh1' })

    expect(apiClient.get).toHaveBeenCalledTimes(1)
    expect(apiClient.get.mock.calls[0][0]).toBe('/meshes/mesh1/devices')
    expect(store.devices).toEqual([{ id: 'd2' }])
  })

  it('calls /zones/{zoneId}/devices when zoneId is provided (overrides mesh route)', async () => {
    const store = useDevicesStore()
    apiClient.get.mockResolvedValueOnce({ data: [{ id: 'd3' }] })

    await store.fetchDevices({ meshId: 'meshX', zoneId: 'zone1' })

    expect(apiClient.get).toHaveBeenCalledTimes(1)
    expect(apiClient.get.mock.calls[0][0]).toBe('/zones/zone1/devices')
    expect(store.devices).toEqual([{ id: 'd3' }])
  })

  it('handles errors by clearing devices and setting error message', async () => {
    const store = useDevicesStore()
    apiClient.get.mockRejectedValueOnce(new Error('network'))

    await store.fetchDevices()

    expect(store.devices).toEqual([])
    expect(store.error).toBe('Failed to fetch devices.')
  })

  it('clearDevices empties the list', async () => {
    const store = useDevicesStore()
    store.devices = [{ id: 'dX' }]

    store.clearDevices()

    expect(store.devices).toEqual([])
  })
})