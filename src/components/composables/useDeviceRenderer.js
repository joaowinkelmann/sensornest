import { ref } from 'vue'
import L from 'leaflet'

export function useDeviceRenderer(map, props, emit) {
  const deviceLayers = ref({})

  const createDeviceMarker = (device, latlng) => {
    const deviceColor = device.deviceColor || '#31CCEC' // Cor padrão por enquanto
    const markerHtml = `
      <div class="device-marker-wrapper">
        <div class="device-marker-pulse" style="background-color: ${deviceColor};"></div>
        <div class="device-marker-dot" style="background-color: ${deviceColor};"></div>
      </div>
    `

    const icon = L.divIcon({
      className: 'device-marker-icon',
      html: markerHtml,
      iconSize: [24, 24],
      iconAnchor: [12, 12],
    })

    const marker = L.marker(latlng, {
      icon,
      draggable: props.editMode === 'devices'
    })

    marker.on('dragend', (event) => {
      emit('device-moved', {
        id: device.id,
        latlng: event.target.getLatLng()
      });
    });

    marker.bindPopup(`
      <div class="device-popup">
        <h4>${device.name}</h4>
        <p><strong>MAC:</strong> ${device.macAddress}</p>
        <p><strong>Status:</strong> ${device.status}</p>
        <p><strong>Role:</strong> ${device.role}</p>
      </div>
    `)

    return marker
  }

  const drawDevices = (devices, conversionFn) => {
    if (!map.value || !devices) return
    clearDeviceMarkers()

    const newLayers = {}
    devices.forEach((device) => {
      const latlng = conversionFn(device)
      if (latlng) {
        const marker = createDeviceMarker(device, latlng)
        marker.addTo(map.value)
        newLayers[device.id] = marker
      }
    })
    deviceLayers.value = newLayers
  }

  const clearDeviceMarkers = () => {
    Object.values(deviceLayers.value).forEach((layer) => {
      if (map.value && map.value.hasLayer(layer)) {
        map.value.removeLayer(layer)
      }
    })
    deviceLayers.value = {}
  }

  return {
    drawDevices,
    clearDeviceMarkers,
  }
}