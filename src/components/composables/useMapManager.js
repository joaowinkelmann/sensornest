import { ref } from 'vue';
import L from 'leaflet';

export function useMapManager(mapContainer) {
  const map = ref(null);

  const initializeMap = () => {
    return new Promise((resolve) => {
      if (map.value) {
        resolve(map.value);
        return;
      }
      map.value = L.map(mapContainer.value, {
        center: [-23.5505, -46.6333],
        zoom: 15,
        zoomControl: true,
        attributionControl: false,
      });
      L.tileLayer('https://{s}.tile.openstreetmap.org/{z}/{x}/{y}.png', {
        maxZoom: 19,
        attribution: '© OpenStreetMap contributors'
      }).addTo(map.value);
      resolve(map.value);
    });
  };

  const destroyMap = () => {
    if (map.value) {
      map.value.remove();
      map.value = null;
    }
  };

  const setMapDragging = (enabled) => {
    if (map.value) {
      if (enabled) {
        map.value.dragging.enable();
        map.value.touchZoom.enable();
        map.value.scrollWheelZoom.enable();
      } else {
        map.value.dragging.disable();
        map.value.touchZoom.disable();
        map.value.scrollWheelZoom.disable();
      }
    }
  };

  return {
    map,
    initializeMap,
    destroyMap,
    setMapDragging,
  };
}