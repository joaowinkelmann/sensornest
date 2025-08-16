// Computed properties for map data conversion
const convertedZones = computed(() =>
  zonesStore.zones.map(zone => {
    if (!zone.vertices || zone.vertices.length === 0) {
      return { ...zone, vertices: [] };
    }
    const xs = zone.vertices.map(v => v.x);
    const ys = zone.vertices.map(v => v.y);
    const centroidX = xs.reduce((a, b) => a + b, 0) / xs.length;
    const centroidY = ys.reduce((a, b) => a + b, 0) / ys.length;

    return {
      ...zone,
      vertices: zone.vertices.map(v => ({
        lat: baseCoords.value.lat + (v.y - centroidY) * SCALE,
        lng: baseCoords.value.lng + (v.x - centroidX) * SCALE,
      })),
    };
  })
);

// Coordinate conversion utilities for vertices
const convertToApiVertex = (latlng) => {
  const activeZone = zonesStore.activeZone;
  if (!activeZone || activeZone.vertices.length === 0) {
    return {
      x: Math.round((latlng.lng - baseCoords.value.lng) / SCALE),
      y: Math.round((latlng.lat - baseCoords.value.lat) / SCALE),
    };
  }

  const xs = activeZone.vertices.map(v => v.x);
  const ys = activeZone.vertices.map(v => v.y);
  const centroidX = xs.reduce((a, b) => a + b, 0) / xs.length;
  const centroidY = ys.reduce((a, b) => a + b, 0) / ys.length;

  return {
    x: Math.round(((latlng.lng - baseCoords.value.lng) / SCALE) + centroidX),
    y: Math.round(((latlng.lat - baseCoords.value.lat) / SCALE) + centroidY),
  };
};

// Coordinate conversion for devices
const convertDeviceToLatLng = (device) => {
  const zone = zonesStore.zones.find(z => z.id === device.zoneId);
  if (!zone || !zone.vertices || zone.vertices.length === 0) {
    // Fallback if zone has no vertices (less accurate)
    return {
      lat: baseCoords.value.lat + device.y * SCALE,
      lng: baseCoords.value.lng + device.x * SCALE,
    };
  }

  const xs = zone.vertices.map(v => v.x);
  const ys = zone.vertices.map(v => v.y);
  const centroidX = xs.reduce((a, b) => a + b, 0) / xs.length;
  const centroidY = ys.reduce((a, b) => a + b, 0) / ys.length;

  return {
    lat: baseCoords.value.lat + (device.y - centroidY) * SCALE,
    lng: baseCoords.value.lng + (device.x - centroidX) * SCALE,
  };
};