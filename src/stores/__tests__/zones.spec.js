import { describe, it, expect, vi, beforeEach } from 'vitest'
import { setActivePinia, createPinia } from 'pinia'

// Mocks for dependent stores used inside zones store
const fetchDevicesMock = vi.fn()
const clearDevicesMock = vi.fn()
const selectedMeshId = 'mesh123'

// Mock API client used by zones store for network calls
vi.mock('@/api', () => {
  const get = vi.fn()
  const post = vi.fn()
  const patch = vi.fn()
  return { default: { get, post, patch } }
})

vi.mock('../devices', () => ({
  useDevicesStore: vi.fn(() => ({
    fetchDevices: fetchDevicesMock,
    clearDevices: clearDevicesMock,
  })),
}))

vi.mock('../meshes', () => ({
  useMeshesStore: vi.fn(() => ({
    selectedMeshId,
  })),
}))

import apiClient from '@/api'
import { useZonesStore } from '../zones'

describe('zones store: active zone, optimistic updates, backup/restore', () => {
  beforeEach(() => {
    setActivePinia(createPinia())
    fetchDevicesMock.mockReset()
    clearDevicesMock.mockReset()
    apiClient.get.mockReset()
    apiClient.post.mockReset()
    apiClient.patch.mockReset()
  })

  it('setActiveZone sets id, resets dirty flag, and fetches devices for current mesh', () => {
    const zones = useZonesStore()

    zones.setActiveZone('zone1')

    expect(zones.activeZoneId).toBe('zone1')
    expect(zones.isZoneDirty).toBe(false)
    expect(fetchDevicesMock).toHaveBeenCalledWith({ meshId: selectedMeshId })
    expect(clearDevicesMock).not.toHaveBeenCalled()
  })

  it('setActiveZone(null) clears devices (after being on a zone)', () => {
    const zones = useZonesStore()

    zones.setActiveZone('z-prev')
    expect(fetchDevicesMock).toHaveBeenCalledWith({ meshId: selectedMeshId })
    fetchDevicesMock.mockClear()

    zones.setActiveZone(null)

    expect(zones.activeZoneId).toBeNull()
    expect(clearDevicesMock).toHaveBeenCalledTimes(1)
    expect(fetchDevicesMock).not.toHaveBeenCalled()
  })

  it('setActiveZone with same id is a no-op (does not re-fetch devices)', () => {
    const zones = useZonesStore()
    zones.setActiveZone('z1')
    fetchDevicesMock.mockClear()

    zones.setActiveZone('z1')

    expect(fetchDevicesMock).not.toHaveBeenCalled()
  })

  it('updateLocalZone mutates zone and marks as dirty', () => {
    const zones = useZonesStore()
    zones.zones = [{ id: 'z1', name: 'Z1', vertices: [{ x: 0, y: 0 }] }]

    zones.updateLocalZone('z1', { vertices: [{ x: 1, y: 1 }, { x: 2, y: 2 }] })

    expect(zones.zones[0].vertices).toEqual([{ x: 1, y: 1 }, { x: 2, y: 2 }])
    expect(zones.isZoneDirty).toBe(true)
  })

  it('updateLocalZone for non-existent zone does not mark dirty', () => {
    const zones = useZonesStore()
    zones.zones = [{ id: 'z1', name: 'Z1' }]

    zones.updateLocalZone('z-unknown', { name: 'Z?' })

    expect(zones.zones[0].name).toBe('Z1')
    expect(zones.isZoneDirty).toBe(false)
  })

  it('backupActiveZone and restoreActiveZone revert unsaved changes', () => {
    const zones = useZonesStore()
    zones.zones = [{ id: 'z1', name: 'Z1', vertices: [{ x: 0, y: 0 }, { x: 10, y: 10 }] }]
    zones.setActiveZone('z1')

    zones.backupActiveZone()
    const original = JSON.parse(JSON.stringify(zones.activeZone.vertices))

    zones.updateLocalZone('z1', { vertices: [{ x: 5, y: 5 }] })
    expect(zones.isZoneDirty).toBe(true)

    zones.restoreActiveZone()

    expect(zones.activeZone.vertices).toEqual(original)
    expect(zones.isZoneDirty).toBe(false)
    expect(zones.backupVertices).toBeNull()
  })

  it('restoreActiveZone without backup does nothing', () => {
    const zones = useZonesStore()
    zones.zones = [{ id: 'z1', vertices: [{ x: 1, y: 1 }] }]
    zones.setActiveZone('z1')
    const before = JSON.stringify(zones.activeZone)

    zones.restoreActiveZone()

    expect(JSON.stringify(zones.activeZone)).toBe(before)
    expect(zones.backupVertices).toBeNull()
  })
})

describe('zones store: networking (fetchZones, fetchAllZones, updateZone, createZone)', () => {
  beforeEach(() => {
    setActivePinia(createPinia())
    fetchDevicesMock.mockReset()
    clearDevicesMock.mockReset()
    apiClient.get.mockReset()
    apiClient.post.mockReset()
    apiClient.patch.mockReset()
  })

  it('fetchZones(null) clears zones and devices without calling API', async () => {
    const zones = useZonesStore()
    zones.zones = [{ id: 'z-old' }]
    await zones.fetchZones(null)

    expect(zones.zones).toEqual([])
    expect(clearDevicesMock).toHaveBeenCalledTimes(1)
    expect(apiClient.get).not.toHaveBeenCalled()
  })

  it('fetchZones success selects first zone and triggers device fetch', async () => {
    const zones = useZonesStore()
    apiClient.get.mockResolvedValueOnce({
      data: [
        { id: 'z1', name: 'A' },
        { id: 'z2', name: 'B' }
      ]
    })

    await zones.fetchZones(selectedMeshId)

    expect(apiClient.get).toHaveBeenCalledWith(`/meshes/${selectedMeshId}/zones`)
    expect(zones.zones.map(z => z.id)).toEqual(['z1', 'z2'])
    expect(zones.activeZoneId).toBe('z1')
    expect(zones.isZoneDirty).toBe(false)
    expect(fetchDevicesMock).toHaveBeenCalledWith({ meshId: selectedMeshId })
  })

  it('fetchZones error sets error and resets state', async () => {
    const zones = useZonesStore()
    zones.zones = [{ id: 'x' }]
    zones.activeZoneId = 'x'
    apiClient.get.mockRejectedValueOnce(new Error('boom'))

    await zones.fetchZones(selectedMeshId)

    expect(zones.error).toBe('Failed to fetch zones.')
    expect(zones.zones).toEqual([])
    expect(zones.activeZoneId).toBeNull()
  })

  it('fetchAllZones success stores data into allZones', async () => {
    const zones = useZonesStore()
    const list = [{ id: 'a' }, { id: 'b' }]
    apiClient.get.mockResolvedValueOnce({ data: list })

    await zones.fetchAllZones()

    expect(apiClient.get).toHaveBeenCalledWith('/zones')
    expect(zones.allZones).toEqual(list)
  })

  it('updateZone success updates server data and clears dirty flag', async () => {
    const zones = useZonesStore()
    zones.zones = [{ id: 'z1', name: 'Before' }]
    zones.updateLocalZone('z1', { name: 'Dirty' })
    expect(zones.isZoneDirty).toBe(true)

    apiClient.patch.mockResolvedValueOnce({ data: { id: 'z1', name: 'Saved' } })

    await zones.updateZone('z1', { name: 'Saved' })

    expect(apiClient.patch).toHaveBeenCalledWith('/zones/z1', { name: 'Saved' })
    expect(zones.zones[0]).toEqual({ id: 'z1', name: 'Saved' })
    expect(zones.isZoneDirty).toBe(false)
  })

  it('createZone success pushes zone, activates it, marks dirty, and fetches devices', async () => {
    const zones = useZonesStore()
    const created = { id: 'zNew', name: 'New', vertices: [] }
    apiClient.post.mockResolvedValueOnce({ data: created })

    const result = await zones.createZone({ name: 'New' })

    expect(apiClient.post).toHaveBeenCalledWith('/zones', { name: 'New' })
    expect(result).toEqual(created)
    expect(zones.zones.find(z => z.id === 'zNew')).toEqual(created)
    expect(zones.activeZoneId).toBe('zNew')
    expect(zones.isZoneDirty).toBe(true)
    expect(fetchDevicesMock).toHaveBeenCalledWith({ meshId: selectedMeshId })
  })
})