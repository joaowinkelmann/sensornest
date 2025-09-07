import { ref } from 'vue';
import L from 'leaflet';

export function useMapManager(mapContainer) {
  const map = ref(null);

  const initializeMap = (options) => {
    const { lat, lon, mapUrl } = options;
    return new Promise((resolve) => {
      if (map.value) {
        destroyMap();
      }

      let mapOptions = {
        zoom: 15,
        zoomControl: true,
        attributionControl: false,
      };

      let tileLayerUrl;
      let tileLayerOptions = {};

      if (mapUrl) {
        mapOptions.crs = L.CRS.Simple;
        mapOptions.center = [0, 0];
        mapOptions.zoom = 1;
        tileLayerUrl = mapUrl;
        tileLayerOptions = {
          minZoom: 0,
          maxZoom: 5,
          attribution: 'Custom Map',
          noWrap: true,
        };
      } else {
        mapOptions.center = [lat, lon];
        tileLayerUrl = 'https://{s}.tile.openstreetmap.org/{z}/{x}/{y}.png';
        tileLayerOptions = {
          maxZoom: 19,
          attribution: '© OpenStreetMap contributors'
        };
      }

      map.value = L.map(mapContainer.value, mapOptions);

      L.tileLayer(tileLayerUrl, tileLayerOptions).addTo(map.value);

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