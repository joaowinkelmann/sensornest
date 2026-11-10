import { ref } from 'vue'
import L from 'leaflet'

export function useDeviceRenderer(map, props, emit) {
  const deviceLayers = ref({})

  const createDeviceMarker = (device, latlng, mode) => {
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
      draggable: mode === 'move-devices'
    });

    if (mode === 'move-devices') {
      marker.on('dragstart', () => {
        const wrapper = marker.getElement().querySelector('.device-marker-wrapper');
        if (wrapper) {
          wrapper.style.transform = 'scale(1.3)';
          wrapper.style.transition = 'transform 0.2s';
        }
        marker.getElement().style.zIndex = '2000';

        // Desabilita temporariamente as interações do mapa apenas enquanto arrasta um dispositivo
        if (map.value) {
          map.value.dragging.disable();
          map.value.touchZoom.disable();
          map.value.scrollWheelZoom.disable();
        }
      });

      marker.on('dragend', (event) => {
        const wrapper = marker.getElement().querySelector('.device-marker-wrapper');
        if (wrapper) {
          wrapper.style.transform = 'scale(1)';
        }
        marker.getElement().style.zIndex = 'auto';

        // Reabilita as interações do mapa após arrastar
        if (map.value) {
          map.value.dragging.enable();
          map.value.touchZoom.enable();
          map.value.scrollWheelZoom.enable();
        }

        emit('device-moved', {
          id: device.id,
          latlng: event.target.getLatLng()
        });
      });
    } else {
      let statsButtonEl = null;

      const handleStatsClick = (event) => {
        event.stopPropagation();
        marker.closePopup();
        emit('device-clicked', device);
      };

      const handleZoneClick = (event) => {
        event.stopPropagation();
        marker.closePopup();
        emit('device-zone-clicked', device);
      };

      const registerStatsButton = (popupEvent) => {
        const popupEl = popupEvent?.popup?.getElement();
        if (!popupEl) return;

        statsButtonEl = popupEl.querySelector('.device-popup-stats');
        if (statsButtonEl) {
          statsButtonEl.addEventListener('click', handleStatsClick);
        }

        const zoneButtonEl = popupEl.querySelector('.device-popup-zone');
        if (zoneButtonEl) {
          zoneButtonEl.addEventListener('click', handleZoneClick);
        }
      };

      const unregisterStatsButton = () => {
        if (statsButtonEl) {
          statsButtonEl.removeEventListener('click', handleStatsClick);
          statsButtonEl = null;
        }
      };

      marker.on('popupopen', registerStatsButton);
      marker.on('popupclose', unregisterStatsButton);
    }

    marker.bindPopup(`
      <div class="device-popup">
        <h4>${device.name}</h4>
        <p><strong>MAC:</strong> ${device.macAddress}</p>
        <p><strong>Status:</strong> ${device.status}</p>
        <p><strong>Role:</strong> ${device.role}</p>
        <div class="device-popup-actions">
          <button
            type="button"
            class="device-popup-button device-popup-stats"
            title="View statistics"
            aria-label="View statistics"
          >
            <span class="material-icons">bar_chart</span>
          </button>
          <button
            type="button"
            class="device-popup-button device-popup-zone"
            title="Change zone"
            aria-label="Change zone"
          >
            <span class="material-icons">layers</span>
          </button>
        </div>
      </div>
    `)

    return marker
  }

  const getModeKey = (mode) => (mode ?? '__default');

  const cleanupMarker = (marker) => {
    if (!marker) return;
    marker.off();
    if (marker.closePopup) {
      marker.closePopup();
    }
    if (map.value && map.value.hasLayer(marker)) {
      map.value.removeLayer(marker);
    }
  };

  const drawDevices = (devices, conversionFn, mode) => {
    if (!map.value || !devices) return;

    const modeKey = getModeKey(mode);
    const existingLayers = deviceLayers.value;
    const nextLayers = {};

    devices.forEach((device) => {
      const rawLatLng = conversionFn(device);
      if (!rawLatLng) return;

      const latlng = L.latLng(rawLatLng);
      const currentMarker = existingLayers[device.id];

      if (currentMarker && currentMarker.__modeKey === modeKey) {
        const currentLatLng = currentMarker.getLatLng();
        if (!currentLatLng.equals(latlng)) {
          currentMarker.setLatLng(latlng);
        }
        nextLayers[device.id] = currentMarker;
        return;
      }

      if (currentMarker) {
        cleanupMarker(currentMarker);
      }

      const marker = createDeviceMarker(device, latlng, mode);
      marker.__modeKey = modeKey;
      marker.addTo(map.value);
      nextLayers[device.id] = marker;
    });

    Object.entries(existingLayers).forEach(([deviceId, marker]) => {
      if (!nextLayers[deviceId]) {
        cleanupMarker(marker);
      }
    });

    deviceLayers.value = nextLayers;
  };

  const clearDeviceMarkers = () => {
    Object.values(deviceLayers.value).forEach(cleanupMarker);
    deviceLayers.value = {};
  }

  return {
    drawDevices,
    clearDeviceMarkers,
  }
}