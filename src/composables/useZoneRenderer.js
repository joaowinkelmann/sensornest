import { ref } from 'vue';
import L from 'leaflet';

export function useZoneRenderer(map, props, emit) {
  const zoneLayers = ref({});
  const vertexMarkers = ref([]);

  const zoneStyles = {
    active: {
      color: '#d97706',
      weight: 3,
      opacity: 1,
      fillOpacity: 0.4,
      fillColor: '#fbbf24'
    },
    inactive: {
      color: '#6b7280',
      weight: 2,
      opacity: 0.7,
      fillOpacity: 0.1,
      fillColor: '#9ca3af'
    },
    hover: {
      fillOpacity: 0.3,
      color: '#f59e0b'
    }
  };

  const vertexMarkerConfigs = {
    remove: {
      className: 'vertex-marker',
      html: `<div style="background-color: #C10015; width: 12px; height: 12px; border-radius: 50%; border: 2px solid white; box-shadow: 0 0 0 2px #C10015;"></div>`,
      iconSize: [24, 24],
      iconAnchor: [12, 12],
    },
    move: {
      className: 'vertex-marker',
      html: `<div style="background-color: #31CCEC; width: 12px; height: 12px; border-radius: 50%; border: 2px solid white; box-shadow: 0 0 0 2px #31CCEC; cursor: grab;"></div>`,
      iconSize: [24, 24],
      iconAnchor: [12, 12],
    }
  };

  const clearVertexMarkers = () => {
    vertexMarkers.value.forEach(marker => {
      if (map.value) map.value.removeLayer(marker);
    });
    vertexMarkers.value = [];
  };

  const createVertexMarker = (latlng, index, mode) => {
    const config = vertexMarkerConfigs[mode];
    if (!config) return null;

    const marker = L.marker(latlng, {
      draggable: mode === 'move',
      icon: L.divIcon(config)
    });

    if (mode === 'remove') {
      marker.on('click', (e) => {
        L.DomEvent.stopPropagation(e);
        emit('vertex-removed', index);
      });
    } else if (mode === 'move') {
      marker.on('dragstart', () => {
        if (map.value) {
          map.value.dragging.disable();
        }
        marker.getElement().style.cursor = 'grabbing';
        marker.getElement().style.transform = 'scale(1.2)';
        marker.getElement().style.zIndex = '1000';
      });

      marker.on('dragend', (e) => {
        if (map.value) {
          map.value.dragging.enable();
        }
        marker.getElement().style.cursor = 'grab';
        marker.getElement().style.transform = 'scale(1)';
        marker.getElement().style.zIndex = 'auto';

        emit('vertex-moved', {
          index: index,
          latlng: e.target.getLatLng()
        });
      });
    }

    return marker;
  };

  const enableEditMode = (zoneId, mode) => {
    clearVertexMarkers();
    if (!mode || mode === 'zones' || mode === 'view' || mode === 'draw-zone') return; // modos de visualização pura ou desenhando nova zona (tratado pelo clique no mapa)

    const zone = props.zones.find(z => z.id === zoneId);
    if (!zone || !zone.vertices) return;

    if (mode === 'remove' || mode === 'move') {
      zone.vertices.forEach((vertex, index) => {
        const marker = createVertexMarker([vertex.lat, vertex.lng], index, mode);
        if (marker && map.value) {
          marker.addTo(map.value);
          vertexMarkers.value.push(marker);
        }
      });
    }
  };

  const createZonePolygon = (zone, isActive) => {
    if (!zone.vertices || zone.vertices.length === 0) return null;

    const latlngs = zone.vertices.map(v => [v.lat, v.lng]);
    const style = isActive ? zoneStyles.active : zoneStyles.inactive;

    const polygon = L.polygon(latlngs, style);

    polygon.bindPopup(`
      <div class="zone-popup">
        <h3>${zone.name}</h3>
        <p><strong>Points:</strong> ${zone.vertices.length}</p>
        <p><strong>ID:</strong> ${zone.id}</p>
        ${zone.description ? `<p><strong>Description:</strong> ${zone.description}</p>` : ''}
      </div>
    `);

    polygon.on('mouseover', function () {
      this.setStyle(zoneStyles.hover);
    });

    polygon.on('mouseout', function () {
      const currentStyle = isActive ? zoneStyles.active : zoneStyles.inactive;
      this.setStyle(currentStyle);
    });

    polygon.on('click', (e) => {
      // Impede a propagação do evento se quisermos apenas selecionar
      L.DomEvent.stopPropagation(e);
      emit('zone-clicked', zone.id);
    });

    return polygon;
  };

  const drawZones = (zones, activeZoneId) => {
    if (!map.value) return;

    // Remove camadas antigas que não estão na nova lista de zonas
    const newZoneIds = new Set(zones.map(z => z.id));
    Object.keys(zoneLayers.value).forEach(id => {
      // Precisa converter id para o tipo correto se necessário (chaves são strings)
      if (!newZoneIds.has(Number(id)) && !newZoneIds.has(String(id))) {
        if (map.value.hasLayer(zoneLayers.value[id])) {
          map.value.removeLayer(zoneLayers.value[id]);
        }
        delete zoneLayers.value[id];
      }
    });

    zones.forEach(zone => {
      const isActive = zone.id === activeZoneId;
      const existingLayer = zoneLayers.value[zone.id];

      // Remove existente se existir, para renderizar novamente
      if (existingLayer && map.value.hasLayer(existingLayer)) {
        map.value.removeLayer(existingLayer);
      }

      const newLayer = createZonePolygon(zone, isActive);
      if (newLayer) {
        newLayer.addTo(map.value);
        zoneLayers.value[zone.id] = newLayer;
      }
    });
  };

  const clearZoneLayers = () => {
    clearVertexMarkers();
    Object.values(zoneLayers.value).forEach(layer => {
      if (map.value && map.value.hasLayer(layer)) {
        map.value.removeLayer(layer);
      }
    });
    zoneLayers.value = {};
  };

  const highlightZone = (zoneId, duration = 2000) => {
    const layer = zoneLayers.value[zoneId];
    if (!layer) return;

    const originalStyle = layer.options;
    const highlightStyle = {
      ...originalStyle,
      color: '#ff6b6b',
      weight: 4,
      fillOpacity: 0.5
    };

    layer.setStyle(highlightStyle);

    setTimeout(() => {
      layer.setStyle(originalStyle);
    }, duration);
  };

  return {
    zoneLayers,
    vertexMarkers,
    drawZones,
    enableEditMode,
    clearVertexMarkers,
    clearZoneLayers,
    highlightZone
  };
}