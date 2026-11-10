import { ref } from 'vue';
import L from 'leaflet';

const CustomTileLayer = L.TileLayer.extend({
  getTileUrl: function (coords) {
    const { x, y } = coords;

    // para quando os tiles são out of bounds
    if (x < 0 || y < 0) {
      return 'data:image/gif;base64,R0lGODlhAQABAIAAAAAAAP///yH5BAEAAAAALAAAAAABAAEAAAIBRAA7';
    }

    // para quando os tiles são validos pega a url
    return L.TileLayer.prototype.getTileUrl.call(this, coords);
  }
});

L.tileLayer.custom = function (url, options) {
  return new CustomTileLayer(url, options);
};

export function useMapManager(mapContainer) {
  const map = ref(null);

  const initializeMap = (options) => {
    const { lat, lon, mesh } = options;
    return new Promise((resolve) => {
      if (map.value) {
        destroyMap();
      }

      let mapOptions = {
        zoom: 15,
        zoomControl: false,
        attributionControl: false,
      };

      let tileLayer;

      if (mesh && mesh.mapUrl) {
        mapOptions.crs = L.CRS.Simple;
        mapOptions.center = [0, 0];
        mapOptions.zoom = 1;

        const ociToken = import.meta.env.VITE_OCI_MAP_TOKEN;

        // pega a url até o /n/
        let baseUrl = mesh.mapUrl.split('/n/')[0];

        // url final é a baseUrl + /p/ + ociToken + o resto da url após o /n/
        // Exemplo: baseUrl + /p/ + ociToken + /n/ + restoDaUrl
        // const finalMapUrl = `${baseUrl}/p/${ociToken}/n/${finalMapUrl.split('/n/')[1]}`;
        const finalMapUrl = ociToken ? `${baseUrl}/p/${ociToken}/n/${mesh.mapUrl.split('/n/')[1]}` : mesh.mapUrl;

        const tileLayerOptions = {
          minZoom: mesh.mapMinZoom ?? mesh.minZoom ?? 0,
          maxZoom: mesh.mapMaxZoom ?? mesh.maxZoom ?? 3,
          attribution: 'Custom Map',
          noWrap: true,
        };

        mapOptions.minZoom = tileLayerOptions.minZoom;
        mapOptions.maxZoom = tileLayerOptions.maxZoom;

        map.value = L.map(mapContainer.value, mapOptions);
        tileLayer = L.tileLayer.custom(finalMapUrl, tileLayerOptions);

        if (mesh.mapHeight && mesh.mapWidth) {
          const southWest = map.value.unproject([0, mesh.mapHeight], tileLayerOptions.maxZoom);
          const northEast = map.value.unproject([mesh.mapWidth, 0], tileLayerOptions.maxZoom);
          const bounds = new L.LatLngBounds(southWest, northEast);

          map.value.setMaxBounds(bounds);
          map.value.fitBounds(bounds);
          tileLayer.options.bounds = bounds;
        }

        map.value.addLayer(tileLayer);
        map.value.setMinZoom(tileLayerOptions.minZoom);
        map.value.setMaxZoom(tileLayerOptions.maxZoom);

      } else {
        mapOptions.center = [lat, lon];
        map.value = L.map(mapContainer.value, mapOptions);
        tileLayer = L.tileLayer('https://{s}.tile.openstreetmap.org/{z}/{x}/{y}.png', {
          maxZoom: 19,
          attribution: '© OpenStreetMap contributors'
        });
      }

      tileLayer.addTo(map.value);

      L.control.zoom({ position: 'topright' }).addTo(map.value);

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