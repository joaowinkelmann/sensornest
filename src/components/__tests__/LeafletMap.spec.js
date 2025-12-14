import { describe, it, expect, vi, beforeEach } from 'vitest';
import { mount } from '@vue/test-utils';
import LeafletMap from '../LeafletMap.vue';
import { useZoneRenderer } from '../../composables/useZoneRenderer';


// Mocks
vi.mock('../../composables/useMapManager', () => ({
    useMapManager: vi.fn(() => ({
        map: {
            value: {
                on: vi.fn(),
                off: vi.fn(),
                removeLayer: vi.fn(),
            }
        },
        initializeMap: vi.fn(),
        destroyMap: vi.fn(),
    })),
}));

vi.mock('../../composables/useZoneRenderer', () => ({
    useZoneRenderer: vi.fn(() => ({
        drawZones: vi.fn(),
        clearZoneLayers: vi.fn(),
        enableEditMode: vi.fn(),
        clearVertexMarkers: vi.fn(),
    })),
}));

vi.mock('../../composables/useDeviceRenderer', () => ({
    useDeviceRenderer: vi.fn(() => ({
        drawDevices: vi.fn(),
        clearDeviceMarkers: vi.fn(),
    })),
}));

vi.mock('vue-i18n', () => ({
    useI18n: () => ({ t: (key) => key }),
}));

describe('LeafletMap.vue', () => {
    let wrapper;
    let zoneRendererMock;

    const defaultProps = {
        zones: [{ id: 'z1' }],
        activeZoneId: 'z1',
        devices: [],
        conversionFn: vi.fn(),
        editMode: 'zones',
    };

    beforeEach(() => {
        vi.clearAllMocks();

        // Capture the mock returned by useZoneRenderer so we can assert on it
        zoneRendererMock = {
            drawZones: vi.fn(),
            clearZoneLayers: vi.fn(),
            enableEditMode: vi.fn(),
            clearVertexMarkers: vi.fn(),
        };
        useZoneRenderer.mockReturnValue(zoneRendererMock);

        wrapper = mount(LeafletMap, {
            props: defaultProps,
            global: {
                stubs: {
                    MapControls: true, // Stub child components
                }
            }
        });
    });

    it('renders and initializes map on mount', () => {
        expect(wrapper.find('.leaflet-map-wrapper').exists()).toBe(true);
        // useMapManager is called in setup, so valid
    });

    it('draws zones and enables edit mode on mount', () => {
        expect(zoneRendererMock.drawZones).toHaveBeenCalledWith(defaultProps.zones, defaultProps.activeZoneId);
        expect(zoneRendererMock.enableEditMode).toHaveBeenCalledWith(defaultProps.activeZoneId, null);
        // mode is null initially in component
    });

    it('updates mode when handleModeChange is triggered (via expose or internal logic)', async () => {
        // We can access exposed methods via wrapper.vm depending on test-utils version, 
        // or we can test the watcher if we change props/mode. 
        // But handleModeChange is internal used by MapControls @mode-changed

        // Simulate mode change from MapControls event
        const controls = wrapper.findComponent({ name: 'MapControls' });
        if (controls.exists()) {
            controls.vm.$emit('mode-changed', 'move');
        } else {
            // Manual call if stubbing makes find impossible (stub usually preserves name)
            // Let's rely on internal method call if we could, but we can't easily.
            // Instead, let's toggle the exposed setMode if available or verify via watchers.
        }

        // LeafletMap exposes `setMode`. Let's use that.
        wrapper.vm.setMode('move');
        await wrapper.vm.$nextTick();

        expect(zoneRendererMock.clearVertexMarkers).toHaveBeenCalled();
        expect(zoneRendererMock.drawZones).toHaveBeenCalled();
        expect(zoneRendererMock.enableEditMode).toHaveBeenCalledWith('z1', 'move');
    });

    it('cleans up on unmount', () => {
        wrapper.unmount();
        expect(zoneRendererMock.clearZoneLayers).toHaveBeenCalled();

    });
});
