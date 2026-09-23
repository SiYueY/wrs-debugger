import { ref } from 'vue';
import { t } from '../i18n';

export function useOperationFeedback(onError: () => void | Promise<void>) {
  const busy = ref(false);
  const message = ref('');

  async function run(action: () => Promise<void>) {
    busy.value = true;
    message.value = '';
    try {
      await action();
      message.value = t('operationSucceeded');
    } catch (error) {
      message.value = error instanceof Error ? error.message : t('operationFailed');
      void onError();
    } finally {
      busy.value = false;
    }
  }

  return { busy, message, run };
}
