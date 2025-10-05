<template>
  <q-page class="q-pa-md">
    <div class="row q-col-gutter-md">
      <div class="col-12 col-md-6 col-lg-4" v-for="item in dashboardItems" :key="item.titleKey">
        <q-card class="cursor-pointer" @click="navigate(item.route)">
          <q-card-section>
            <div class="text-h6">{{ t(item.titleKey) }}</div>
            <div class="text-subtitle2">{{ t(item.captionKey) }}</div>
          </q-card-section>
          <q-card-actions align="right">
            <q-btn flat color="primary" :label="t(item.actionLabelKey)" />
          </q-card-actions>
        </q-card>
      </div>
    </div>
  </q-page>
</template>

<script setup>
import { useI18n } from 'vue-i18n';
import { useRouter } from 'vue-router';

const router = useRouter();
const { t } = useI18n();

const dashboardItems = [
  {
    titleKey: 'home.meshes.title',
    captionKey: 'home.meshes.caption',
    actionLabelKey: 'home.meshes.actionLabel',
    route: '/meshes'
  },
  {
    titleKey: 'home.reports.title',
    captionKey: 'home.reports.caption',
    actionLabelKey: 'home.reports.actionLabel',
    route: '/reports'
  },
  {
    titleKey: 'home.data.title',
    captionKey: 'home.data.caption',
    actionLabelKey: 'home.data.actionLabel',
    route: '/data'
  },
  {
    titleKey: 'home.mapEditor.title',
    captionKey: 'home.mapEditor.caption',
    actionLabelKey: 'home.mapEditor.actionLabel',
    route: '/map-editor'
  }
];

const navigate = (route) => {
  if (route) {
    router.push(route);
  }
};
</script>
