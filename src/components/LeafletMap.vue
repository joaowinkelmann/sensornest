<template>
  <!-- This div is the container where the Leaflet map will be rendered. -->
  <div ref="mapContainer" class="leaflet-map-container"></div>
</template>

<script setup>
import { ref, onMounted, onUnmounted, watch } from 'vue';
import L from 'leaflet';

// Define props to receive data from the parent component
const props = defineProps({
  zones: {
    type: Array,
    required: true,
  },
  activeZoneId: {
    type: String,
    default: null,
  },
  mode: String, // 'add', 'remove', 'move', or null
});

// Define the events that this component can emit to its parent
const emit = defineEmits(['vertex-added', 'vertex-removed', 'vertex-moved']);

const mapContainer = ref(null);
let map = null;
let zoneLayers = {};

// This function enables or disables map dragging and other interactions
const setMapDragging = (enabled) => {
  if (!map) return;
  if (enabled) {
    map.dragging.enable();
    map.touchZoom.enable();
    map.doubleClickZoom.enable();
    map.scrollWheelZoom.enable();
    map.boxZoom.enable();
    map.keyboard.enable();
  } else {
    map.dragging.disable();
    map.touchZoom.disable();
    map.doubleClickZoom.disable();
    map.scrollWheelZoom.disable();
    map.boxZoom.disable();
    map.keyboard.disable();
  }
};

// This function draws/updates the zone polygons on the map
const drawZones = (zones) => {
  if (!map) return;

  const existingZoneIds = Object.keys(zoneLayers);
  const newZoneIds = zones.map(z => z.id);

  // Remove layers for zones that no longer exist
  existingZoneIds.forEach(zoneId => {
    if (!newZoneIds.includes(zoneId)) {
      map.removeLayer(zoneLayers[zoneId]);
      delete zoneLayers[zoneId];
    }
  });

  if (zones && zones.length > 0) {
    zones.forEach(zone => {
      const isActive = zone.id === props.activeZoneId;
      const style = {
        color: isActive ? '#d97706' : '#3388ff', // Amber for active, blue for others
        weight: isActive ? 3 : 2,
        opacity: 1,
        fillOpacity: 0.2,
      };

      if (Array.isArray(zone.vertices) && zone.vertices.length > 0) {
        const latlngs = zone.vertices.map(v => [v.lat, v.lng]);
        
        if (zoneLayers[zone.id]) {
          // Update existing layer
          zoneLayers[zone.id].setLatLngs(latlngs);
          zoneLayers[zone.id].setStyle(style);
        } else {
          // Create new layer
          const polygon = L.polygon(latlngs, style).bindPopup(`<b>${zone.name}</b>`);
          polygon.addTo(map);
          zoneLayers[zone.id] = polygon;
        }

        // Remove old markers
        if (zoneLayers[zone.id]._vertexMarkers) {
          zoneLayers[zone.id]._vertexMarkers.forEach(marker => map.removeLayer(marker));
        }
        zoneLayers[zone.id]._vertexMarkers = [];

        // Add vertex markers for remove/move mode (only for active zone)
        if (isActive && (props.mode === 'remove' || props.mode === 'move')) {
          zone.vertices.forEach((v, idx) => {
            let markerOptions = {};
            if (props.mode === 'remove') {
              markerOptions = {
                icon: L.divIcon({
                  className: 'vertex-remove-marker',
                  html: '<span style="color:red;font-weight:bold;">R</span>',
                  iconSize: [24, 24],
                  iconAnchor: [12, 12],
                }),
                interactive: true,
              };
            } else if (props.mode === 'move') {
              markerOptions = {
                draggable: true,
                icon: L.divIcon({
                  className: 'vertex-move-marker',
                  html: '<span style="color:blue;font-weight:bold;">M</span>',
                  iconSize: [24, 24],
                  iconAnchor: [12, 12],
                }),
                interactive: true,
              };
            }
            const marker = L.marker([v.lat, v.lng], markerOptions);
            if (props.mode === 'remove') {
              marker.on('click', (e) => {
                emit('vertex-removed', idx);
              });
            } else if (props.mode === 'move') {
              marker.on('dragstart', () => setMapDragging(false));
              marker.on('dragend', (e) => {
                setMapDragging(true);
                const newLatLng = e.target.getLatLng();
                emit('vertex-moved', { index: idx, latlng: newLatLng });
              });
              marker.on('click', () => setMapDragging(false));
              marker.on('mouseup', () => setMapDragging(true));
            }
            marker.addTo(map);
            zoneLayers[zone.id]._vertexMarkers.push(marker);
          });
        }
      }
    });

    // Fit the map view to the bounds of all drawn polygons
    const allLayers = Object.values(zoneLayers).filter(l => l instanceof L.Polygon);
    if (allLayers.length > 0) {
      const group = new L.featureGroup(allLayers);
      map.fitBounds(group.getBounds().pad(0.1));
    }
  }
};

// onMounted is a Vue lifecycle hook that runs after the component is added to the DOM
onMounted(() => {
  if (mapContainer.value) {
    // Initialize the map
    map = L.map(mapContainer.value, {
      zoomControl: true,
    }).setView([-23.5505, -46.6333], 13); // Centered on São Paulo

    // Add the tile layer (the map background) from OpenStreetMap
    L.tileLayer('https://{s}.tile.openstreetmap.org/{z}/{x}/{y}.png', {
      attribution: '&copy; <a href="https://www.openstreetmap.org/copyright">OpenStreetMap</a> contributors',
    }).addTo(map);

    // Event listener for when the map is clicked
    map.on('click', (event) => {
      if (props.activeZoneId && props.mode === 'add') {
        emit('vertex-added', event.latlng);
      }
    });

    // Initial drawing of zones passed via props
    drawZones(props.zones);
  }
});

// onUnmounted is a lifecycle hook that cleans up when the component is destroyed
onUnmounted(() => {
  if (map) {
    map.remove();
    map = null;
  }
});

// Watch for changes to the 'mode' prop to enable/disable map interactions
watch(() => props.mode, (newMode) => {
  setMapDragging(newMode !== 'move');
});

// Watch for changes to the 'zones' prop and redraw the polygons on the map
watch(() => [props.zones, props.mode, props.activeZoneId], () => {
  drawZones(props.zones);
}, { deep: true });
</script>

<style>
.leaflet-map-container {
  width: 100%;
  min-height: 500px;
  height: 100%;
}
.vertex-remove-marker {
  cursor: pointer;
  background: white;
  border-radius: 50%;
  border: 2px solid red;
  display: flex;
  align-items: center;
  justify-content: center;
  width: 24px;
  height: 24px;
}
.vertex-move-marker {
  cursor: grab;
  background: white;
  border-radius: 50%;
  border: 2px solid blue;
  display: flex;
  align-items: center;
  justify-content: center;
  width: 24px;
  height: 24px;
}
</style>