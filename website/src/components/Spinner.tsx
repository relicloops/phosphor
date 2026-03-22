import { css } from '@relicloops/cathode';

interface SpinnerProps {
  message?: string;
}

export const Spinner = ( { message = 'Loading...' }: SpinnerProps ) => {
  css( './css/components/spinner.css' );

  return (
    <div class="spinner-container">
      <div class="spinner">
        <div class="spinner-ring"></div>
        <div class="spinner-ring"></div>
        <div class="spinner-ring"></div>
      </div>
      {message && <div class="spinner-message">{message}</div>}
    </div>
  );
};
