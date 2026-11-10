import { describe, it, expect, vi, beforeEach } from 'vitest';
import { ref } from 'vue';
import { useZoneRenderer } from '../useZoneRenderer';
import L from 'leaflet';

// Mock Leaflet
vi.mock('leaflet', () => {
    const map = {
        addLayer: vi.fn(),
        removeLayer: vi.fn(),
        hasLayer: vi.fn(),
        dragging: {
            enable: vi.fn(),
            disable: vi.fn(),
        },
    };

    const markerStub = {
        addTo: vi.fn((m) => m && m.addLayer && m.addLayer(markerStub)),
        on: vi.fn(),
        getElement: vi.fn(() => ({ style: {} })),
    };

    const polygonStub = {
        addTo: vi.fn((m) => m && m.addLayer && m.addLayer(polygonStub)),
        bindPopup: vi.fn(),
        on: vi.fn(),
        setStyle: vi.fn(),
        options: {},
    };

    return {
        default: {
            marker: vi.fn(() => markerStub),
            divIcon: vi.fn(),
            polygon: vi.fn(() => polygonStub),
            DomEvent: {
                stopPropagation: vi.fn(),
            },
            map: vi.fn(() => map),
        },
    };
});

describe('useZoneRenderer', () => {
    let map;
    let props;
    let emit;
    let renderer;

    beforeEach(() => {
        map = ref({
            addLayer: vi.fn(),
            removeLayer: vi.fn(),
            hasLayer: vi.fn(),
            dragging: {
                enable: vi.fn(),
                disable: vi.fn(),
            },
        });

        props = {
            zones: [
                { id: 'z1', name: 'Zone 1', vertices: [{ lat: 0, lng: 0 }, { lat: 10, lng: 10 }, { lat: 0, lng: 10 }] }
            ]
        };

        emit = vi.fn();

        renderer = useZoneRenderer(map, props, emit);
        vi.clearAllMocks();
    });

    it('drawZones creates polygons and adds them to the map', () => {
        renderer.drawZones(props.zones, 'z1');

        expect(L.polygon).toHaveBeenCalledTimes(1);
        expect(map.value.addLayer).toHaveBeenCalledTimes(1);
        expect(renderer.zoneLayers.value['z1']).toBeDefined();
    });

    it('drawZones updates existing zones if called again', () => {
        renderer.drawZones(props.zones, 'z1');
        expect(L.polygon).toHaveBeenCalledTimes(1);

        // Call again
        renderer.drawZones(props.zones, 'z1');
        // Should call polygon creation again for fresh layer
        expect(L.polygon).toHaveBeenCalledTimes(2);

        // Should remove old layer if it was added (we mocked hasLayer to return false by default, let's fix that validation logic assumption if needed)
        // The implementation removes if existingLayer is present and on map.
    });

    it('enableEditMode adds vertex markers for move mode', () => {
        renderer.enableEditMode('z1', 'move');

        expect(L.marker).toHaveBeenCalledTimes(3); // 3 vertices
        expect(map.value.addLayer).toHaveBeenCalledTimes(3);
        expect(renderer.vertexMarkers.value.length).toBe(3);
    });

    it('enableEditMode adds vertex markers for remove mode', () => {
        renderer.enableEditMode('z1', 'remove');

        expect(L.marker).toHaveBeenCalledTimes(3);
        expect(map.value.addLayer).toHaveBeenCalledTimes(3);
    });

    it('enableEditMode does nothing for view mode', () => {
        renderer.enableEditMode('z1', 'view');

        expect(L.marker).not.toHaveBeenCalled();
        expect(map.value.addLayer).not.toHaveBeenCalled();
        expect(renderer.vertexMarkers.value.length).toBe(0);
    });

    it('clearZoneLayers removes all layers and markers', () => {
        // Setup some state
        renderer.drawZones(props.zones, 'z1');
        renderer.enableEditMode('z1', 'move');

        // Mock that map has these layers so removal logic triggers
        map.value.hasLayer.mockReturnValue(true);

        renderer.clearZoneLayers();

        expect(map.value.removeLayer).toHaveBeenCalled();
        expect(Object.keys(renderer.zoneLayers.value).length).toBe(0);
        expect(renderer.vertexMarkers.value.length).toBe(0);
    });

    it('clearVertexMarkers only removes markers', () => {
        renderer.enableEditMode('z1', 'move');
        const markerCount = renderer.vertexMarkers.value.length;
        expect(markerCount).toBe(3);

        renderer.clearVertexMarkers();

        expect(map.value.removeLayer).toHaveBeenCalledTimes(3);
        expect(renderer.vertexMarkers.value.length).toBe(0);
    });
});
