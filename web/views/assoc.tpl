%include('header.tpl', title="", refresh=10)

%include('menu.tpl', current='assoc')

%if len(senders) == 0:
<p>No active Songcast Receivers found</p>
%elif  len(receivers) == 0:
<p>No Songcast Receivers found</p>
%else:

<form action="/assoc" method="POST">

<h3>Choose Sender</h3>
<table>
  <tr>
    <th>Select</th><th>Name</th><th>UUID</th><th>Sender URI</th>
  </tr>
%for sender in senders:
  <tr>
    <td><input type="radio" name="Sender" value="{{sender[1]}}"/></td>
    <td>{{sender[0]}}</td>
    <td>{{sender[1]}}</td>
    <td>{{sender[2]}}</td>
  </tr>
%end
</table>

<p></p>

<h3>Choose Receivers to associate</h3>

<table>
  <tr>
    <th>Select</th><th>Name</th><th>State</th><th>UUID</th><th>Uri</th>
  </tr>
%for receiver in receivers:
  <tr>
    <td><input type="checkbox" name="Assoc" value="{{receiver[2]}}"/></td>
    <td>{{receiver[0]}}</td>
    <td>{{receiver[1]}}</td>
    <td>{{receiver[2]}}</td>
    <td>{{receiver[3]}}</td>
  </tr>
%end
</table>

<p></p>

<input type="submit" value="Link selected receivers to selected Sender">
</form>

%end

%include('footer.tpl')
