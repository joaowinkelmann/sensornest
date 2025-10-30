import { ref } from 'vue';
import L from 'leaflet';

export function useZoneRenderer(map, props, emit) {
  const zoneLayers = ref({});

  // Zone styling configuration
  const zoneStyles = {
    active: {
      color: '#d97706', // A vibrant orange for the border
      weight: 3,
      opacity: 1,
      fillOpacity: 0.4,
      fillColor: '#fbbf24' // A matching amber/yellow for the fill
    },
    inactive: {
      color: '#6b7280', // A neutral gray for the border
      weight: 2,
      opacity: 0.7,
      fillOpacity: 0.1,
      fillColor: '#9ca3af' // A lighter gray for the fill
    },
    hover: {
      fillOpacity: 0.3,
      color: '#f59e0b' // A brighter orange on hover
    }
  };

  // Vertex marker configurations
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

  /**
   * Create a vertex marker for editing
   */
  const createVertexMarker = (latlng, index, mode) => {
    const config = vertexMarkerConfigs[mode];
    if (!config) return null;

    const marker = L.marker(latlng, {
      draggable: mode === 'move',
      icon: L.divIcon(config)
    });

    // Add event handlers based on mode
    if (mode === 'remove') {
      marker.on('click', (e) => {
        e.originalEvent?.stopPropagation();
        emit('vertex-removed', index);
      });
    } else if (mode === 'move') {
      marker.on('dragstart', () => {
        // Disable map dragging while moving vertex
        if (map.value) {
          map.value.dragging.disable();
          map.value.touchZoom.disable();
          map.value.scrollWheelZoom.disable();
        }
        marker.getElement().style.cursor = 'grabbing';
        marker.getElement().style.transform = 'scale(1.2)';
        marker.getElement().style.zIndex = '1000';
      });

      marker.on('dragend', (e) => {
        // Re-enable map interactions
        if (map.value) {
          map.value.dragging.enable();
          map.value.touchZoom.enable();
          map.value.scrollWheelZoom.enable();
        }
        marker.getElement().style.cursor = 'grab';
        marker.getElement().style.transform = 'scale(1)';
        marker.getElement().style.zIndex = 'auto';

        emit('vertex-moved', {
          index: index,
          latlng: e.target.getLatLng()
        });
      });

      marker.on('drag', () => {
        // Optional: emit real-time updates during drag
        // emit('vertex-moving', { index: index, latlng: e.target.getLatLng() });
      });
    }

    return marker;
  };

  /**
   * Create a zone polygon layer
   */
  const createZonePolygon = (zone, isActive) => {
    if (!zone.vertices || zone.vertices.length === 0) {
      return null;
    }

    const latlngs = zone.vertices.map(v => [v.lat, v.lng]);
    const style = isActive ? zoneStyles.active : zoneStyles.inactive;

    const polygon = L.polygon(latlngs, style);

    // Add popup with zone information
    polygon.bindPopup(`
      <div class="zone-popup">
        <h3>${zone.name}</h3>
        <p><strong>Points:</strong> ${zone.vertices.length}</p>
        <p><strong>ID:</strong> ${zone.id}</p>
        ${zone.description ? `<p><strong>Description:</strong> ${zone.description}</p>` : ''}
      </div>
    `);

    // Add hover effects
    polygon.on('mouseover', function () {
      this.setStyle(zoneStyles.hover);
    });

    polygon.on('mouseout', function () {
      const currentStyle = isActive ? zoneStyles.active : zoneStyles.inactive;
      this.setStyle(currentStyle);
    });

    // Add click handler for zone selection
    polygon.on('click', function () {
      emit('zone-clicked', zone.id);
    });

    return polygon;
  };

  /**
   * Clear all vertex markers for a zone
   */
  const clearVertexMarkers = (zoneId) => {
    const zoneLayer = zoneLayers.value[zoneId];
    if (zoneLayer && zoneLayer._vertexMarkers) {
      zoneLayer._vertexMarkers.forEach(marker => {
        if (map.value && map.value.hasLayer(marker)) {
          map.value.removeLayer(marker);
        }
      });
      zoneLayer._vertexMarkers = [];
    }
  };

  /**
   * Add vertex markers for editing
   */
  const addVertexMarkers = (zone, mode) => {
    if (!zone.vertices || zone.vertices.length === 0 || !mode) {
      return;
    }

    const zoneLayer = zoneLayers.value[zone.id];
    if (!zoneLayer) return;

    // Clear existing markers
    clearVertexMarkers(zone.id);

    // Create new markers
    const markers = zone.vertices.map((vertex, index) => {
      const marker = createVertexMarker([vertex.lat, vertex.lng], index, mode);
      if (marker && map.value) {
        marker.addTo(map.value);
      }
      return marker;
    }).filter(Boolean);

    zoneLayer._vertexMarkers = markers;
  };

  /**
   * Update a single zone layer
   */
  const updateZoneLayer = (zone, isActive, mode) => {
    const existingLayer = zoneLayers.value[zone.id];

    // Remove existing layer if it exists
    if (existingLayer) {
      clearVertexMarkers(zone.id);
      if (map.value && map.value.hasLayer(existingLayer)) {
        map.value.removeLayer(existingLayer);
      }
    }

    // Create new layer
    const newLayer = createZonePolygon(zone, isActive);
    if (newLayer && map.value) {
      newLayer.addTo(map.value);
      zoneLayers.value[zone.id] = newLayer;

      // Add vertex markers if in editing mode and this is the active zone
      if (isActive && (mode === 'remove' || mode === 'move')) {
        addVertexMarkers(zone, mode);
      }
    }
  };

  /**
   * Main function to draw/update all zones
   */
  const drawZones = (zones, activeZoneId, mode) => {
    if (!map.value || !zones) return;

    const existingZoneIds = Object.keys(zoneLayers.value);
    const newZoneIds = zones.map(z => z.id.toString());

    // Remove layers for zones that no longer exist
    existingZoneIds.forEach(zoneId => {
      if (!newZoneIds.includes(zoneId)) {
        const layer = zoneLayers.value[zoneId];
        if (layer) {
          clearVertexMarkers(zoneId);
          if (map.value.hasLayer(layer)) {
            map.value.removeLayer(layer);
          }
          delete zoneLayers.value[zoneId];
        }
      }
    });

    // Update or create layers for current zones
    zones.forEach(zone => {
      const isActive = zone.id === activeZoneId;
      updateZoneLayer(zone, isActive, mode);
    });
  };

  /**
   * Get zone layer by ID
   */
  const getZoneLayer = (zoneId) => {
    return zoneLayers.value[zoneId];
  };

  /**
   * Highlight a specific zone temporarily
   */
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
    drawZones,
    getZoneLayer,
    highlightZone
  };
}