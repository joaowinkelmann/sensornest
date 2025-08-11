<template>
  <!-- This div is the container where the Leaflet map will be rendered. -->
  <div ref="mapContainer" class="h-full w-full"></div>
</template>

<script setup>
import { ref, onMounted, onUnmounted, watch } from 'vue';
import L from 'leaflet';
import 'leaflet-draw';

// Define props to receive data from the parent component (ZoneManager)
const props = defineProps({
  zones: {
    type: Array,
    required: true,
  },
});

// Define the event that this component can emit to its parent
const emit = defineEmits(['zone-created']);

const mapContainer = ref(null);
let map = null;
let drawnItems = null;

// This function draws the existing zone polygons on the map
const drawZones = (zones) => {
  if (!map || !drawnItems) return;

  drawnItems.clearLayers(); // Clear previous polygons before drawing new ones

  if (zones && zones.length > 0) {
    zones.forEach(zone => {
      // Ensure vertices exist and is an array
      if (Array.isArray(zone.vertices) && zone.vertices.length > 0) {
        const latlngs = zone.vertices.map(v => [v.lat, v.lng]);
        const polygon = L.polygon(latlngs, { color: '#3388ff' }).bindPopup(`<b>${zone.name}</b>`);
        drawnItems.addLayer(polygon);
      }
    });

    // Fit the map view to the bounds of all drawn polygons
    if (drawnItems.getLayers().length > 0) {
      map.fitBounds(drawnItems.getBounds().pad(0.1));
    }
  }
};

// onMounted is a Vue lifecycle hook that runs after the component is added to the DOM
onMounted(() => {
  if (mapContainer.value) {
    // Initialize the map
    map = L.map(mapContainer.value).setView([-23.5505, -46.6333], 13); // Centered on São Paulo

    // Add the tile layer (the map background) from OpenStreetMap
    L.tileLayer('https://{s}.tile.openstreetmap.org/{z}/{x}/{y}.png', {
      attribution: '&copy; <a href="https://www.openstreetmap.org/copyright">OpenStreetMap</a> contributors',
    }).addTo(map);

    // Initialize the feature group that will hold our drawn polygons
    drawnItems = new L.FeatureGroup();
    map.addLayer(drawnItems);

    // Initialize the Leaflet Draw control
    const drawControl = new L.Control.Draw({
      edit: {
        featureGroup: drawnItems, // Allow editing of the polygons in this group
      },
      draw: {
        polygon: true, // Allow drawing polygons
        polyline: false,
        rectangle: false,
        circle: false,
        marker: false,
        circlemarker: false,
      },
    });
    map.addControl(drawControl);

    // Event listener for when a new shape is created
    map.on(L.Draw.Event.CREATED, (event) => {
      const layer = event.layer;
      const vertices = layer.getLatLngs()[0].map(latlng => ({ lat: latlng.lat, lng: latlng.lng }));
      
      const zoneName = prompt("Enter a name for the new zone:", "New Zone");
      
      if (zoneName) {
        // Emit the 'zone-created' event to the parent component with the new zone's data
        emit('zone-created', { name: zoneName, vertices });
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

// Watch for changes to the 'zones' prop and redraw the polygons on the map
watch(() => props.zones, (newZones) => {
  drawZones(newZones);
}, { deep: true }); // 'deep: true' is important to detect changes within the array of objects
</script>