import { describe, it, expect, vi, beforeEach } from 'vitest'
import { setActivePinia, createPinia } from 'pinia'

const fetchDevicesMock = vi.fn()
const clearDevicesMock = vi.fn()
const selectedMeshId = 'mesh123'

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

describe('store de zones: zona ativa, atualizações otimistas, backup/restauração', () => {
  beforeEach(() => {
    setActivePinia(createPinia())
    fetchDevicesMock.mockReset()
    clearDevicesMock.mockReset()
    apiClient.get.mockReset()
    apiClient.post.mockReset()
    apiClient.patch.mockReset()
  })

  it('setActiveZone define id, redefine flag dirty e busca dispositivos para a malha atual', () => {
    const zones = useZonesStore()

    zones.setActiveZone('zone1')

    expect(zones.activeZoneId).toBe('zone1')
    expect(zones.isZoneDirty).toBe(false)
    expect(fetchDevicesMock).toHaveBeenCalledWith({ meshId: selectedMeshId })
    expect(clearDevicesMock).not.toHaveBeenCalled()
  })

  it('setActiveZone(null) limpa dispositivos (após estar em uma zona)', () => {
    const zones = useZonesStore()

    zones.setActiveZone('z-prev')
    expect(fetchDevicesMock).toHaveBeenCalledWith({ meshId: selectedMeshId })
    fetchDevicesMock.mockClear()

    zones.setActiveZone(null)

    expect(zones.activeZoneId).toBeNull()
    expect(clearDevicesMock).toHaveBeenCalledTimes(1)
    expect(fetchDevicesMock).not.toHaveBeenCalled()
  })

  it('setActiveZone com mesmo id é uma operação nula (não busca dispositivos novamente)', () => {
    const zones = useZonesStore()
    zones.setActiveZone('z1')
    fetchDevicesMock.mockClear()

    zones.setActiveZone('z1')

    expect(fetchDevicesMock).not.toHaveBeenCalled()
  })

  it('updateLocalZone altera a zona e marca como dirty', () => {
    const zones = useZonesStore()
    zones.zones = [{ id: 'z1', name: 'Z1', vertices: [{ x: 0, y: 0 }] }]

    zones.updateLocalZone('z1', { vertices: [{ x: 1, y: 1 }, { x: 2, y: 2 }] })

    expect(zones.zones[0].vertices).toEqual([{ x: 1, y: 1 }, { x: 2, y: 2 }])
    expect(zones.isZoneDirty).toBe(true)
  })

  it('updateLocalZone para zona inexistente não marca como dirty', () => {
    const zones = useZonesStore()
    zones.zones = [{ id: 'z1', name: 'Z1' }]

    zones.updateLocalZone('z-unknown', { name: 'Z?' })

    expect(zones.zones[0].name).toBe('Z1')
    expect(zones.isZoneDirty).toBe(false)
  })

  it('backupActiveZone e restoreActiveZone revertem alterações não salvas', () => {
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

  it('restoreActiveZone sem backup não faz nada', () => {
    const zones = useZonesStore()
    zones.zones = [{ id: 'z1', vertices: [{ x: 1, y: 1 }] }]
    zones.setActiveZone('z1')
    const before = JSON.stringify(zones.activeZone)

    zones.restoreActiveZone()

    expect(JSON.stringify(zones.activeZone)).toBe(before)
    expect(zones.backupVertices).toBeNull()
  })
})

describe('store de zones: rede (fetchZones, fetchAllZones, updateZone, createZone)', () => {
  beforeEach(() => {
    setActivePinia(createPinia())
    fetchDevicesMock.mockReset()
    clearDevicesMock.mockReset()
    apiClient.get.mockReset()
    apiClient.post.mockReset()
    apiClient.patch.mockReset()
  })

  it('fetchZones(null) limpa zonas e dispositivos sem chamar API', async () => {
    const zones = useZonesStore()
    zones.zones = [{ id: 'z-old' }]
    await zones.fetchZones(null)

    expect(zones.zones).toEqual([])
    expect(clearDevicesMock).toHaveBeenCalledTimes(1)
    expect(apiClient.get).not.toHaveBeenCalled()
  })

  it('fetchZones sucesso seleciona primeira zona e aciona busca de dispositivos', async () => {
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

  it('fetchZones erro define erro e redefine estado', async () => {
    const zones = useZonesStore()
    zones.zones = [{ id: 'x' }]
    zones.activeZoneId = 'x'
    apiClient.get.mockRejectedValueOnce(new Error('boom'))

    await zones.fetchZones(selectedMeshId)

    expect(zones.error).toBe('Falha ao buscar zones.')
    expect(zones.zones).toEqual([])
    expect(zones.activeZoneId).toBeNull()
  })

  it('fetchAllZones sucesso armazena dados em allZones', async () => {
    const zones = useZonesStore()
    const list = [{ id: 'a' }, { id: 'b' }]
    apiClient.get.mockResolvedValueOnce({ data: list })

    await zones.fetchAllZones()

    expect(apiClient.get).toHaveBeenCalledWith('/zones')
    expect(zones.allZones).toEqual(list)
  })

  it('updateZone sucesso atualiza dados no servidor e limpa flag dirty', async () => {
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

  it('createZone sucesso adiciona zona, ativa-a, marca dirty e busca dispositivos', async () => {
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