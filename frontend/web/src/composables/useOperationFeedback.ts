import { ref } from 'vue';
import { t } from '../i18n';
import { useDebuggerStore } from '../stores/debugger';

export function useOperationFeedback(onError: () => void | Promise<void>) {
  const store = useDebuggerStore();
  const busy = ref(false);

  function notify(message: string, error = false) {
    store.showNotification(message, error);
  }

  async function run<T>(
    action: () => Promise<T>,
    successMessage: string | ((result: T) => string) = t('operationSucceeded'),
  ) {
    busy.value = true;
    try {
      const result = await action();
      notify(typeof successMessage === 'function' ? successMessage(result) : successMessage);
    } catch (error) {
      notify(error instanceof Error ? error.message : t('operationFailed'), true);
      void onError();
    } finally {
      busy.value = false;
    }
  }

  return { busy, notify, run };
}
